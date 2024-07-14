

# [gfxreconstruct](https://github.com/LunarG/gfxreconstruct)



可以使用 gfxrecon-convert  将 gfxr 文件转为 json 文件，从 json 文件中可以看到每一次的 vulkan 的 api 的调用和参数信息

```bash
gfxrecon-convert - A tool to convert the contents of GFXReconstruct capture files to JSON.

Usage:
  gfxrecon-convert [-h | --help] [--version] <file>

Required arguments:
  <file>                Path to the GFXReconstruct capture file to be converted
                        to text.

Optional arguments:
  -h                    Print usage information and exit (same as --help).
  --version             Print version information and exit.
  --output file         'stdout' or a path to a file to write JSON output
                        to. Default is the input filepath with "gfxr" replaced by "json".
  --format <format>     JSON format to write.
           json         Standard JSON format (indented)
           jsonl        JSON lines format (every object in a single line)
  --include-binaries    Dump binaries from Vulkan traces in a separate file with an unique name. The main JSON file
                        will include a reference with the file name. The binary files are dumped in a subdirectory
  --expand-flags        Print flags values from Vulkan traces with its correspondent symbolic representation. Otherwise,
                        the flags are printed as hexadecimal value.
  --file-per-frame      Creates a new file for every frame processed. Frame number is added as a suffix
                        to the output file name.
```
