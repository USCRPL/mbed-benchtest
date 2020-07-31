
## Documentation

To save space, this package does not contain documentation files. The current CMSIS documentation is available from [keil.com](http://www.keil.com/cmsis).

## Examples

To save space, this package does not contain the original ARM examples.

## Original files

The ARM original files are kept in the repository `originals` branch, updated with each new release, and merged into the `xpack` branch (three-way merge).

The current files were extracted from the `ARM.CMSIS-RTOS_Validation.1.0.0.pack` archive.

To save space, the following folders/files were removed:

* Documents
* Examples

## Changes

The actual files used by this package are in the `xpack` repository branch.

## Warnings

To silence warnings, the following options must be configured for the Source folder:

```
-Wno-aggregate-return -Wno-conversion -Wno-unused-parameter \
-Wno-missing-prototypes -Wno-missing-declarations -Wno-sign-compare \
-Wno-maybe-uninitialized -Wno-unused-function -Wno-format -Wno-padded
```

## Tests

* none
