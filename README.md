# Noise-Immune FAT32 File system
## Main goal
The main goal of this project is to make the FAT32 file system work in usage scenarios where `SEU` is common. </br>
Also this FS should work on embedded systems for my another project called [CDBMS](https://github.com/j1sk1ss/CordellDBMS.PETPRJ).

## Why?
- Firstly, I have my own project that needs a noise-immune FS.
- Secondly, we currently have ECC RAM, but we have no way to protect data in non-ROM memory from SEU.
- Thirdly, it will be cheaper if we can use software solutions instead of hardware ones.

## Current state
- Base implementation.
- Complete porting from `CordellOS` project.
- Implement noise-immune algorithms.
- Create format tool for creating modified FS on Flashs.
- Testing and comparing with other FS.

## References
FAT32 carcase was taken from my OS project [CordellOS](https://github.com/j1sk1ss/CordellOS.PETPRJ). FS API itself was built with help of [OSDev](https://wiki.osdev.org/Expanded_Main_Page) tips, like this [topic](https://wiki.osdev.org/FAT).

