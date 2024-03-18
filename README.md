# LaTeXiT-metadata

Code used by [IguanaTex](https://github.com/Jonathan-LeRoux/IguanaTex/) to extract information from displays generated with [LaTeXiT](https://www.chachatelier.fr/latexit/) on Mac and convert them into IguanaTex displays.

The end user should not need to fiddle with this code, but we make it public for reference.

This repository provides the Windows code. The Mac version is available on the sister repository [LaTeXiT-metadata-MacOS](https://github.com/LaTeXiT-metadata/LaTeXiT-metadata-MacOS).

# Acknowledgement
LaTeXiT-metadata was kindly prepared by Pierre Chatelier, LaTeXiT's author, to meet IguanaTex's requirements. Many thanks to him!

# Usage

`LaTeXiT-metadata.exe` takes as input a PDF or EMF file with encoded LaTeXiT source information, and creates a `.tex` file containing the encoded LaTeXiT source.

```
LaTeXiT-metadata.exe file.pdf/file.emf
```
will create a file `file.tex` with the LaTeX source.

Note that LaTeXiT itself does not create EMF files, but PowerPoint does in fact save PDF files internally as EMF files. 

When using IguanaTex to edit a LaTeX display generated by LaTeXiT and inserted into a PowerPoint presentation as PDF, IguanaTex will extract the corresponding EMF file and run `LaTeXiT-metadata.exe` on it to retrieve the original LaTeX source.

# Binaries

Compiled executable and binaries are available on the [IguanaTex repo](https://github.com/Jonathan-LeRoux/IguanaTex): [LaTeXiT-metadata-Win.zip](https://github.com/Jonathan-LeRoux/IguanaTex/releases/download/v1.60.3/LaTeXiT-metadata-Win.zip).

# Building LaTeXiT-metadata

Here are very rough instructions on how to build your own LaTeXiT-metadata executable.

A Visual Studio solution is provided.

LaTeXiT-metadata depends on podofo and zlib, so if you want to use your own binaries, you will need to:
- Compile podofo and zlib
- Build `podofo-0.9.7` and `zlib-1.2.11`
- Copy 
  * `podofo-0.9.7\out\install\x64-Release\include\*` to `dependencies\podofo\include\`
  * `podofo-0.9.7\out\install\x64-Release\bin\podofo.dll` to `dependencies\podofo\bin\x64\`
  * `podofo-0.9.7\out\install\x64-Release\lib\podofo.lib` to `dependencies\podofo\lib\x64\`
  * `zlib-1.2.11\out\install\x64-Release\include\*`  to `dependencies\zlib\include\`
  * `zlib-1.2.11\out\install\x64-Release\bin\zlib.dll`  to `dependencies\zlib\bin\x64\`
  * `zlib-1.2.11\out\install\x64-Release\lib\zlib*.lib`  to `dependencies\zlib\lib\x64\`
- Build the project in Visual Studio
