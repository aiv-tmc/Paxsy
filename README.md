# Paxsi
An open-source programming language. The language is currently under active development.

The **paxsi** project is freely available under the MIT license. You can read it at [this link](https://github.com/aiv-tmc/Paxsi/blob/main/LICENSE).

<!--Status of versions-->
## Status of versions
Newest: [paxsi_3.5.11](https://github.com/aiv-tmc/Paxsi/paxsi_3.5.11)

Current: [pxi 3.4f6](https://github.com/aiv-tmc/Paxsi/pxi-3.4w2f6)

Stable: [PXI 3.4](https://github.com/aiv-tmc/Paxsi/PXI-3.4)

<!--Install-->
## Install (Linux)
You must have installed [project dependencies](https://github.com/aiv-tmc/Paxsi#dependencies)

1. Cloning a repository
```git clone https://github.com/aiv-tmc/Paxsi.git```

2. Going to the directory with the code
```cd Paxsi/src/```

3. Compile the prototype
```gcc error_manager.c lexer.c parser.c main.c -o pxi```

4. Launch a project
```./pxi -c <sourse.px>```

<!--Documentation-->
## Documentation
The documentation can be obtained at [this link](./docs/doc-en.md).

<!--Dependencies-->
## Dependencies 
This program depends on the **gcc** interpreter version **15.1** or higher.

---

*Previous versions of the programming language can be viewed here: https://github.com/aiv-tmc/MyLang*
