"""
Parses a vendor_cmds YAML and produces c code for each operation.

Based on cci_vendor_cmds_May31.yaml, this parses the YAML and constructs a
large portion of the necessary c code for implementation. Updated versions of
the affected source files are created and placed into cxlcli-test/cxl/gen
including libcxl.c, libcxl.h, libcxl.sym, builtin.h, cxl.c and memdev.c.

It requires some marked up base versions of these files to read in as
templates, which are all included in the tar.

This is currently a first draft, so it has some limitations:
 - Variable-length input / output payloads are not yet implemented.
 - Names for variables & flags use mnemonics verbatim and are not truncated.
 - Code is inserted directly into the relevant files instead of creating
   vendor specific source files to import. These files are duplicated, not
   overwritten, so it's fine for now but not ideal.
 - The traversal for the pyyml output is a bit hacky, it'll need to be made
   more robust in order to be extended to YAMLs from different vendors.
 - Input parameters greater than 8 bytes of length need to be implemented
   manually.

Instructions for use:
 1. $ tar git clone git@github.com:elake/ndctl.git
 2. $ cd ndctl/cligen
 3. $ python3 cligen.py
 4. Output files will be in ndctl/cligen/gen
"""

import re
import sys
import os

# Only required to run without buck in fb environment:
USER = os.environ.get('USER')
try:
    sys.path.append(f"/data/users/{USER}/fbsource/third-party/pypi/pyyaml/5.4.1/lib3/")
except:
    pass

import yaml

YAMFILE = f"gen/cci_vendor_cmds_May31.yaml"
OUTDIR = f"gen/"
BASE = "base"
BUILTINH = "builtin.h"
CXLC = "cxl.c"
LIBCXLC = "libcxl.c"
LIBCXLH = "libcxl.h"
LIBCXLSYM = "libcxl.sym"
MEMDEVC = "memdev.c"
SIMPLE = True # Only process commands with simple payloads (1, 2, 4, 8 byte param types)
BLANK = False

class Payload():
    """
    Payload class
    """
    def __init__(self, payload, input=True):
        if input:
            x = "i"
        else:
            x = "o"
        self.fixed_size = True
        self.simple = True
        self.var_opl = False
        self.payload = payload
        self.name = payload.get(f'{x}pl_name', "")
        self.mn = payload.get(f'{x}pl_mnemonic', "").lower()
        self.size = payload.get(f'{x}pl_size_bytes')
        if isinstance(self.size, str):
            self.fixed_size = False
            return
        self.params = []
        self.build_params(payload, x)
        self.is_simple()

    def build_params(self, payload, x):
        used_mn = set()
        self.params_used = False
        for par in payload.get('parameters', []):
            mn = par.get(f"{x}pl_par_mnemonic", "").lower()
            if mn != 'rsvd':
                self.params_used = True
            if mn in used_mn:
                offset = par.get(f"{x}pl_offset")
                mn = f"{mn}{offset}"
            used_mn.add(mn)
            param = {
                "name": par.get(f"{x}pl_par_name"),
                "mn": mn,
                "size": par.get(f"{x}pl_length"),
                "offset": par.get(f"{x}pl_offset"),
                "description": par.get(f"{x}pl_description"),
                "enums": [],
                "format_specifier": "",
                "unit_size": par.get(f"{x}pl_unit_size"),
                "contiguous": par.get(f"{x}pl_contiguous", False),
            }
            param.update({
                "type": self.types(param),
            })
            enums = par.get("enumerators", [])
            for en in enums:
                param["enums"].append( {
                    "value": en.get(f"{x}pl_value"),
                    "name": en.get(f"{x}pl_en_name"),
                    "mn": en.get(f"{x}pl_en_mnemnonic")
                })
            param["enums"].sort(key=lambda x:x.get("value"))
            if enums:
                param["format_specifier"] = r"%s"
            else:
                t =  param.get("type")
                if not isinstance(t, str):
                    t = t[0]
                param["format_specifier"] = self.get_format_specifier(t)
            t = param.get("type")
            if not isinstance(t, str):
                t = t[0]
            param["utype"] = re.sub("__le", "u", t)
            self.params.append(param)

    def get_variable_length(self, mn):
        keywords = ["num", "inst"]

    def is_simple(self):
        for param in self.params:
            if param.get("size") not in {1, 2, 4, 8}:
                self.simple = False
                return False
        self.simple = True
        return True

    def get_format_specifier(self, t: str):
        """
        Given a type, return a c format specifier. Currently trivial.
        """
        return {
            "u8" : r"%x",
            "__le16" : r"%x",
            "__le32" : r"%x",
            "__le64" : r"%lx",
        }.get(t, r"%x")

    def types(self, param):
        """
        Given a parameter dict, return an appropriate c type. For large sizes
        or sizes not root 2, return a size and an array length in a tuple.
        """
        i = param.get(f"size")
        unit = param.get("unit_size")
        t = {
            1: 'u8',
            2: '__le16',
            4: '__le32',
            8: '__le64',
            }
        if unit:
            return (t.get(unit), i // unit)
        if t.get(i): return t.get(i)
        if i % 8 == 0:
            return ('__le64', i // 8)
        elif i % 4 == 0:
            return ('__le32', i // 4)
        elif i % 2 == 0:
            return ('__le16', i // 2)
        else:
            return ('u8', i)

    def declaration(self, param, u=False):
        """
        Return a declaration string for a parameter.
        If param['type'] is string: "u8 some_name;"
        If param['type'] is tuple: "u8 some_name[3];"
        """
        if u:
            t = param.get("utype")
        else:
            t = param.get("type")
        mn = param.get("mn")
        if  isinstance(t, str):
            return f"{t} {mn};\n"
        else:
            return f"{t[0]} {mn}[{t[1]}];\n"

def base(s):
    return os.path.join(OUTDIR, f"{BASE}.{s}")

def to_cpu(t, mn):
    t = re.sub("_", "", t)
    if t == 'u8':
        return mn
    else:
        return f"{t}_to_cpu({mn})"

def cpu_to(t, mn):
    t = re.sub("_", "", t)
    if t == 'u8':
        return mn
    else:
        return f"cpu_to_{t}({mn})"

def generate_ipl_struct(name, ipl):
    # struct memdev.c line 138-143
    pname = f"{name}_params"
    out = f"static struct _{pname} {{\n"
    for param in ipl.params:
        mn = param.get('mn')
        l = param.get('size')
        if re.match(r"^rsvd\d*$", mn):
            continue
        if l < 5:
            out += f"\tu32 {mn};\n"
        else:
            out += f"\tu64 {mn};\n"
    out += f"\tbool verbose;\n"
    out += f"}} {pname};\n\n"
    return out


def generate_option_struct(name, ipl):
    # struct memdev.c line 153-157
    out = f"static const struct option cmd_{name}_options[] = {{\n"
    out += f"\t{name.upper()}_BASE_OPTIONS(),\n"
    if ipl.params_used:
        out += f"\t{name.upper()}_OPTIONS(),\n"
    out += f"\tOPT_END(),\n}};\n\n"
    return out

def generate_def_base_options(name):
    # Line 145 memdev.c
    return f'#define {name.upper()}_BASE_OPTIONS() \\\nOPT_BOOLEAN(\'v\',"verbose", &{name}_params.verbose, "turn on debug")\n\n'

def generate_def_options(name, ipl):
    if not ipl.params_used:
        return ""
    # options memdev.c line 148-151
    flags = set()
    t = {
        'u8': 'OPT_UINTEGER',
        '__le16': 'OPT_UINTEGER',
        '__le32': 'OPT_UINTEGER',
        '__le64': 'OPT_U64',
        }
    out = f"#define {name.upper()}_OPTIONS()"
    for param in ipl.params:
        mn = param.get('mn')
        if re.match(r"^rsvd\d*$", mn):
            continue
        pname = param.get('name')
        pt = param.get('type')
        if not isinstance(pt, str):
            pt = pt[0]
        # ctype = t.get(pt)
        if int(param.get('size')) < 5:
            ctype = 'OPT_UINTEGER'
        else:
            ctype = 'OPT_U64'
        flag = mn[0]
        while flag in flags:
            i = ord(flag) + 1
            if i > 122:
                i = 65
            flag = chr(i)
        flags.add(flag)
        out += f" \\\n{ctype}(\'{flag}\', \"{mn}\", &{name}_params.{mn}, \"{pname}\"),"
    out = f"{out.rstrip(',')}\n\n"
    return out

def generate_action_cmd(name, ipl):
    # action_cmd memdev.c line 315-325
    out = f"static int action_cmd_{name}(struct cxl_memdev *memdev, struct action_context *actx)\n{{\n"
    out += f"\tif (cxl_memdev_is_active(memdev)) {{\n"
    out += f"\t\tfprintf(stderr, \"%s: memdev active, abort {name}\\n\",\n"
    out += f"\t\t\tcxl_memdev_get_devname(memdev));\n"
    out += f"\t\treturn -EBUSY;\n\t}}\n\n"
    rout = f"\treturn cxl_memdev_{name}(memdev"
    for param in ipl.params:
        mn = param.get('mn')
        if re.match(r"^rsvd\d*$", mn):
            continue
        t = param.get('type')
        amp = "(void *) "
        if isinstance(t, str):
            # Prepend with & only if parameter is an array
            amp = ""
        if len(rout) > 60:
            out += f"{rout},\n\t\t"
            rout = f"{amp}{name}_params.{mn}"
            continue
        rout += f", {amp}{name}_params.{mn}"
    out += f"{rout});\n}}\n"
    return out


def generate_cmd_def(name):
    # memdev.c line 715-721
    return (
        f"int cmd_{name}(int argc, const char **argv, struct cxl_ctx *ctx)\n{{\n"
        + f"\tint rc = memdev_action(argc, argv, ctx, action_cmd_{name}, cmd_{name}_options,\n"
        + f'\t\t\t"cxl {name} <mem0> [<mem1>..<memN>] [<options>]");\n\n'
        + f"\treturn rc >= 0 ? 0 : EXIT_FAILURE;\n}}\n"
    )


def generate_mem_cmd_info(name, opcode, ipl, opl):
    # libcxl.c line 1927-1930
    name = name.upper()
    out = f"#define CXL_MEM_COMMAND_ID_{name} CXL_MEM_COMMAND_ID_RAW\n"
    out += f"#define CXL_MEM_COMMAND_ID_{name}_OPCODE {opcode}\n"
    if ipl.size:
        out += f"#define CXL_MEM_COMMAND_ID_{name}_PAYLOAD_IN_SIZE {ipl.size}\n"
    if opl.size:
        out += f"#define CXL_MEM_COMMAND_ID_{name}_PAYLOAD_OUT_SIZE {opl.size}\n"
    return out


def generate_mbox(name, payload, end="in"):
    # struct cxl_mbox libcxl.c line 1537-1546 & 1460-1471
    if not payload.params:
        return ""
    out = f"struct cxl_mbox_{name}_{end} {{\n"
    for param in payload.params:
        out += f"\t{payload.declaration(param)}"
    out += f"}}  __attribute__((packed));\n"
    return out


def generate_cxl_export(name, ipl, opl, fullname):
    # CXL_EXPORT libcxl.c line 1549-1619
    out = ""
    nout = f"CXL_EXPORT int cxl_memdev_{name}(struct cxl_memdev *memdev"
    for param in ipl.params:
        mn = param.get('mn')
        if re.match(r"^rsvd\d*$", mn):
            continue
        t = param.get('type')
        if len(nout) > 60:
            out += f"{nout},\n\t"
            nout = ""
            if  isinstance(t, str):
                nout += f"{re.sub('__le', 'u', t)} {mn}"
            else:
                nout += f"{re.sub('__le', 'u', t[0])} *{mn}"
            continue
        if  isinstance(t, str):
            nout += f", {re.sub('__le', 'u', t)} {mn}"
        else:
            nout += f", {re.sub('__le', 'u', t[0])} *{mn}"
    out += f"{nout})\n{{\n"
    for param in opl.params:
        enums = param.get("enums")
        if enums:
            mn = param.get("mn")
            out += f"\tconst char *{mn}_descriptions[] = {{"
            for en in enums:
                out += f"\n\t\t\"{en.get('name')}\","
            out = f"{out.rstrip(',')}\n\t}};\n\n"
    out += f"\tstruct cxl_cmd *cmd;\n"
    if ipl.params_used:
        out += f"\tstruct cxl_mem_query_commands *query;\n"
        out += f"\tstruct cxl_command_info *cinfo;\n"
        out += f"\tstruct cxl_mbox_{name}_in *{name}_in;\n"
    if opl.params:
        out += f"\tstruct cxl_mbox_{name}_out *{name}_out;\n"
    out += f"\tint rc = 0;\n\n"
    out += f"\tcmd = cxl_cmd_new_raw(memdev, CXL_MEM_COMMAND_ID_{name.upper()}_OPCODE);\n"
    out += f"\tif (!cmd) {{\n"
    out += f"\t\tfprintf(stderr, \"%s: cxl_cmd_new_raw returned Null output\\n\",\n"
    out += f"\t\t\t\tcxl_memdev_get_devname(memdev));\n"
    out += f"\t\treturn -ENOMEM;\n\t}}\n\n"
    if ipl.params_used:
        out += f"\tquery = cmd->query_cmd;\n"
        out += f"\tcinfo = &query->commands[cmd->query_idx];\n\n"
        out += f"\t/* update payload size */\n"
        out += f"\tcinfo->size_in = CXL_MEM_COMMAND_ID_{name.upper()}_PAYLOAD_IN_SIZE;\n"
        out += f"\tif (cinfo->size_in > 0) {{\n"
        out += f"\t\t cmd->input_payload = calloc(1, cinfo->size_in);\n"
        out += f"\t\tif (!cmd->input_payload)\n"
        out += f"\t\t\treturn -ENOMEM;\n"
        out += f"\t\tcmd->send_cmd->in.payload = (u64)cmd->input_payload;\n"
        out += f"\t\tcmd->send_cmd->in.size = cinfo->size_in;\n\t}}\n\n"
        out += f"\t{name}_in = (void *) cmd->send_cmd->in.payload;\n\n"
        for param in ipl.params:
            mn = param.get('mn')
            if re.match(r"^rsvd\d*$", mn):
                continue
            t = param.get('type')
            if  isinstance(t, str):
                t = re.sub("_", "", t)
                if t == 'u8':
                    out += f"\t{name}_in->{mn} = {mn};\n"
                else:
                    out += f"\t{name}_in->{mn} = cpu_to_{t}({mn});\n"
            else:
                tz = re.sub("_", "", t[0])
                out += f"\tfor(int i = 0; i < {t[1]}; i++) {{\n"
                if tz == 'u8':
                    out += f"\t\t{name}_in->{mn}[i] = {mn}[i];\n\t}}\n\n"
                else:
                    out += f"\t\t{name}_in->{mn}[i] = cpu_to_{tz}({mn}[i]);\n\t}}\n\n"
    out += f"\trc = cxl_cmd_submit(cmd);\n"
    out += f"\tif (rc < 0) {{\n"
    out += f"\t\tfprintf(stderr, \"%s: cmd submission failed: %d (%s)\\n\",\n"
    out += f"\t\t\t\tcxl_memdev_get_devname(memdev), rc, strerror(-rc));\n"
    out += f"\t\t goto out;\n\t}}\n\n"
    out += f"\trc = cxl_cmd_get_mbox_status(cmd);\n"
    out += f"\tif (rc != 0) {{\n"
    out += f"\t\tfprintf(stderr, \"%s: firmware status: %d\\n\",\n"
    out += f"\t\t\t\tcxl_memdev_get_devname(memdev), rc);\n"
    out += f"\t\trc = -ENXIO;\n"
    out += f"\t\tgoto out;\n\t}}\n\n"
    out += f"\tif (cmd->send_cmd->id != CXL_MEM_COMMAND_ID_{name.upper()}) {{\n"
    out += f"\t\t fprintf(stderr, \"%s: invalid command id 0x%x (expecting 0x%x)\\n\",\n"
    out += f"\t\t\t\tcxl_memdev_get_devname(memdev), cmd->send_cmd->id, CXL_MEM_COMMAND_ID_{name.upper()});\n"
    out += f"\t\treturn -EINVAL;\n\t}}\n\n"
    if opl.params:
        out += f"\t{name}_out = (void *)cmd->send_cmd->out.payload;\n"
        heading = ( (78 - len(fullname)) // 2 ) * '='
        heading += f" {fullname} "
        heading += ( 80 - len(heading) ) * '='
        out += f"\tfprintf(stdout, \"{heading}\\n\");\n"
        for param in opl.params:
            fs = param.get('format_specifier')
            mn = param.get('mn')
            if re.match(r"^rsvd\d*$", mn):
                continue
            t = param.get('type')
            if isinstance(t, str):
                print_value = to_cpu(t, f'{name}_out->{mn}')
                if not param.get("enums"):
                    out += f"\tfprintf(stdout, \"{param.get('name')}: {fs}\\n\", {print_value});\n"
                else:
                    out += f"\tfprintf(stdout, \"{param.get('name')}: {fs}\\n\", {mn}_descriptions[{name}_out->{mn}]);\n"
            else:
                out += f"\tfprintf(stdout, \"{param.get('name')}: \");\n"
                out += f"\t/* Procedurally generated print statement. To print this array contiguously,\n\t   add \"contiguous: True\" to the YAML param and rerun cligen.py */\n"
                out += f"\tfor (int i = 0; i < {t[1]}; i++) {{\n"
                if param.get("contiguous"):
                    out += f"\t\tfprintf(stdout, \"{fs}\", {to_cpu(t[0], f'{name}_out->{mn}[i]')});\n\t}}\n"
                else:
                    out += f"\t\tfprintf(stdout, \"{mn}[%d]: {fs}\\n\", i, {to_cpu(t[0], f'{name}_out->{mn}[i]')});\n\t}}\n"
                out += f"\tfprintf(stdout, \"\\n\");\n"
    out += f"\nout:\n"
    out += f"\tcxl_cmd_unref(cmd);\n"
    out += f"\treturn rc;\n"
    out += f"\treturn 0;\n}}\n\n"
    return out


def generate_libcxl_h(name, ipl):
    # cxl_memdev libcxl.h line 62-63
    out = ""
    nout = f"int cxl_memdev_{name}(struct cxl_memdev *memdev"
    for param in ipl.params:
        mn = param.get('mn')
        if re.match(r"^rsvd\d*$", mn):
            continue
        t = param.get('type')
        if len(nout) > 60:
            out += f"{nout},\n\t"
            nout = ""
            if  isinstance(t, str):
                nout += f"{re.sub('__le', 'u', t)} {mn}"
            else:
                nout += f"{re.sub('__le', 'u', t[0])} *{mn}"
            continue
        if  isinstance(t, str):
            nout += f", {re.sub('__le', 'u', t)} {mn}"
        else:
            nout += f", {re.sub('__le', 'u', t[0])} *{mn}"
    out += f"{nout});\n"
    return out

def generate_libcxl_sym(name):
    # libcxl.sym line 75
    return f"\tcxl_memdev_{name};\n"

def build_results(results):
    bb = open(base(BUILTINH), 'r')
    bc = open(base(CXLC), 'r')
    blc = open(base(LIBCXLC), 'r')
    blh = open(base(LIBCXLH), 'r')
    bls = open(base(LIBCXLSYM), 'r')
    bm = open(base(MEMDEVC), 'r')
    b = open(os.path.join(OUTDIR, BUILTINH), 'w')
    c = open(os.path.join(OUTDIR, CXLC), 'w')
    lc = open(os.path.join(OUTDIR, LIBCXLC), 'w')
    lh = open(os.path.join(OUTDIR, LIBCXLH), 'w')
    ls = open(os.path.join(OUTDIR, LIBCXLSYM), 'w')
    m = open(os.path.join(OUTDIR, MEMDEVC), 'w')

    rei = r".* insert here .*/"
    rep = r".* insert here params options .*/"
    rea = r".* insert here action .*/"
    rec = r".* insert here cmd .*/"
    res = r"} LIBCXL_3;"

    for line in bb.readlines():
        if re.search(rei, line):
            for v in results.get("builtin_h").values():
                b.write(v)
        else:
            if not BLANK: b.write(line)
    bb.close()
    b.close()

    for line in bc.readlines():
        if re.search(rei, line):
            for v in results.get("cxl_c").values():
                c.write(v)
        else:
            if not BLANK: c.write(line)
    bc.close()
    c.close()

    for line in blc.readlines():
        if re.search(rei, line):
            for v in results.get("mem_cmd_info").keys():
                lc.write(results.get("mem_cmd_info").get(v))
                lc.write(f"\n")
                lc.write(results.get("mbox_in").get(v))
                lc.write(f"\n")
                lc.write(results.get("mbox_out").get(v))
                lc.write(f"\n")
                lc.write(results.get("cxl_export").get(v))
                lc.write(f"\n")
        else:
            if not BLANK: lc.write(line)
    blc.close()
    lc.close()

    for line in bm.readlines():
        if re.search(rep, line):
            for v in results.get("param_structs_memdev_c").keys():
                m.write(results.get("param_structs_memdev_c").get(v))
                m.write(results.get("base_options_memdev_c").get(v))
                m.write(results.get("options_memdev_c").get(v))
                m.write(results.get("option_structs_memdev_c").get(v))
        elif re.search(rea, line):
            for v in results.get("action_cmd_memdev_c").values():
                m.write(v)
                m.write(f"\n")
        elif re.search(rec, line):
            for v in results.get("cmd_memdev_c").values():
                m.write(v)
                m.write(f"\n")
        else:
            if not BLANK: m.write(line)
    bm.close()
    m.close()

    for line in blh.readlines():
        if re.search(rei, line):
            for v in results.get("libcxl_h").values():
                lh.write(v)
        else:
            if not BLANK: lh.write(line)
    blh.close()
    lh.close()

    for line in bls.readlines():
        if re.search(res, line):
            for v in results.get("libcxl_sym").values():
                ls.write(v)
        if not BLANK: ls.write(line)
    bls.close()
    ls.close()

def run(opcodes):

    cxl_c = {}
    builtin_h = {}
    param_structs_memdev_c = {}
    option_structs_memdev_c = {}
    base_options_memdev_c = {}
    options_memdev_c = {}
    action_cmd_memdev_c = {}
    cmd_memdev_c = {}
    mem_cmd_info = {}
    mbox_in = {}
    mbox_out = {}
    cxl_export = {}
    libcxl_h = {}
    libcxl_sym = {}
    skipped = {}

    for command in opcodes:
        name = command.get("opcode_name", "").lower()
        opcode = command.get("opcode")
        mnemonic = command.get("mnemonic", "").lower()
        description = command.get("opcode_description")
        ipl = Payload(command.get("input_payload", [{}])[0], input=True)
        if SIMPLE and not ipl.simple:
            continue
        opl = Payload(command.get("output_payload", [{}])[0], input=False)
        if not (ipl.fixed_size and opl.fixed_size):
            skipped.update({name: { "ipl.size": ipl.size, "opl.size": opl.size}})
            continue
        cxl_c_cmd_struct = f'\t{{ "{re.sub("_", "-", mnemonic)}", .c_fn = cmd_{mnemonic} }},\n'
        cxl_c[name] = cxl_c_cmd_struct
        builtin_h_cmd = (
            f"int cmd_{mnemonic}(int argc, const char **argv, struct cxl_ctx *ctx);\n"
        )
        builtin_h[name] = builtin_h_cmd
        param_struct = generate_ipl_struct(mnemonic, ipl)
        param_structs_memdev_c[name] = param_struct
        base_options_def = generate_def_base_options(mnemonic)
        base_options_memdev_c[name] = base_options_def
        options_def = generate_def_options(mnemonic, ipl)
        options_memdev_c[name] = options_def
        option_struct = generate_option_struct(mnemonic, ipl)
        option_structs_memdev_c[name] = option_struct
        action_cmd = generate_action_cmd(mnemonic, ipl)
        action_cmd_memdev_c[name] = action_cmd
        cmd_def = generate_cmd_def(mnemonic)
        cmd_memdev_c[name] = cmd_def
        mem_command_info = generate_mem_cmd_info(
            mnemonic, opcode, ipl, opl
        )
        mem_cmd_info[name] = mem_command_info
        cxl_mbox_in = generate_mbox(mnemonic, ipl, end='in')
        mbox_in[name] = cxl_mbox_in
        cxl_mbox_out = generate_mbox(mnemonic, opl, end='out')
        mbox_out[name] = cxl_mbox_out
        libcxl_export = generate_cxl_export(mnemonic, ipl, opl, name)
        cxl_export[name] = libcxl_export
        libh = generate_libcxl_h(mnemonic, ipl)
        libcxl_h[name] = libh
        libsym = generate_libcxl_sym(mnemonic)
        libcxl_sym[name] = libsym
    results = {
        "cxl_c" : cxl_c,
        "builtin_h" : builtin_h,
        "param_structs_memdev_c" : param_structs_memdev_c,
        "option_structs_memdev_c" : option_structs_memdev_c,
        "base_options_memdev_c" : base_options_memdev_c,
        "options_memdev_c" : options_memdev_c,
        "action_cmd_memdev_c" : action_cmd_memdev_c,
        "cmd_memdev_c" : cmd_memdev_c,
        "mem_cmd_info" : mem_cmd_info,
        "mbox_in" : mbox_in,
        "mbox_out" : mbox_out,
        "cxl_export" : cxl_export,
        "libcxl_h" : libcxl_h,
        "libcxl_sym" : libcxl_sym,
        "skipped" : skipped,
    }
    build_results(results)
    print("done.")

with open(YAMFILE, "r") as f:
    yml = yaml.load(f)
    yml_no_decode = yaml.load(f, Loader=yaml.BaseLoader)
    command_sets = yml.get('command_sets')
    opcodes = []
    for cs in command_sets:
        opcodes += cs.get('command_set_opcodes')
    run(opcodes)
