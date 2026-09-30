# Constant registration validation

For the focused adaptation of upstream PR #123968, capture the existing baseline
editor before rebuilding. Use separate absolute output directories:

```sh
mkdir -p /absolute/before /absolute/after
/path/to/baseline/editor --headless --verbose --path /absolute/before \
    --dump-extension-api-with-docs > /absolute/before/api-dump.log 2>&1
# Build the candidate editor once with tests enabled, then:
/path/to/candidate/editor --headless --test --test-case='*ConstantRegistration*'
/path/to/candidate/editor --headless --verbose --path /absolute/after \
    --dump-extension-api-with-docs > /absolute/after/api-dump.log 2>&1
python3 misc/constant_registration_validation/compare_api.py /absolute/before /absolute/after
```

The comparison checks the entire parsed API (including enum names, bitfield
metadata, constant values, method metadata, and documentation) and both ClassDB
API hashes. Focused native tests cover qualified names, enum/bitfield property
metadata, signed 64-bit values, the old GDType inheritance/maps, and every global
constant's map/index/enum membership. The old `get_slice("::", 1)` rule is retained.

This is a risk-based validation, not a complete platform/export matrix. It does
not measure stripped release size or startup performance. For precise savings,
compare immutable baseline `96caefacf36fa83b5cf8f1639ca0cfa47bde3c18` against the
constant-registration commit with matching compiler, minimal-extra profile,
LTO, stripping, dependencies, and source-identification settings. The historical
29.89 MiB release binary predates other changes and is not a valid isolated PR
baseline. No frozen shipping artifact should be overwritten for this check.
