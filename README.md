# png-hide

This C program allows you to inject custom data into a PNG file by adding a custom chunk. It takes in a PNG file and a data file, and outputs a modified PNG with the injected data and extract it when you ever you need.


## Compilation

To compile the program, simply run:

```bash
make
```

This will generate an executable file named `program`.

## Usage

The program expects two arguments:

1.  The path to the PNG file.
2.  The path to the data file to be injected.

```bash
./program <file.png> <data_file>
```

### Inject Example

Inject data from `data.txt` into `test.png`:

```bash
./program test.png data.txt
```
The modified PNG file will be saved as `out.png`.

### Extract Example

Extract data from `test.png`:

```bash
./program test.png
```
The Extracted data file will be written to `stdout`.

## Notes

-   Ensure the input PNG file is valid.
-   The data file can contain any arbitrary data to be added as a custom chunk in the PNG.

## License

This project is open source — feel free to modify and share!

## Resources

[PNG specification](http://www.libpng.org/pub/png/spec/1.2/PNG-Contents.html)
