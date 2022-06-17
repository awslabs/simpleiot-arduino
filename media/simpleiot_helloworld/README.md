# Using JPEG images in IOT EduKit Arduino code

In the M5 Arduino platform, images can be stored on flash or be converted into
C code files which are then included in the code.

The more reliable method is to do the code conversion.

The included Python script `jpeg2code.py` will perform this step. To invoke it, you need to
run it with:

```
python3 jpeg2code.py {jpeg-file-name}
```

This will output a file called `{jpeg-file-name}.c` (with a C suffix added).

This can be embedded in code. Note that in the Arduino M5 LCD library, you need to
provide the byte-size of the C data structure to the `drawJpg` function. This can be found
in the first line of the generated C file.

The file `old-bin2code2.py` is an older version of this and had to be modified to work
with jpg files. It is kept here in case it is needed for reference.
