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

 Note that we have not yet had the chance to test any of the generated code on
 a pioneer card yet, so it has not been validated and may require some manual
 modification after being generated. Expect this to be fixed once we have
 access to a device for testing.
