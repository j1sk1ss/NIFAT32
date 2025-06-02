# Noise-Immune FAT32 File system
## Main goal
The main goal of this project is to make the FAT32 file system work in usage scenarios where `SEU` is common. </br>
Also this FS should work on embedded systems for my another project called [CDBMS](https://github.com/j1sk1ss/CordellDBMS.PETPRJ).

## Why?
- Firstly, I have my own project that needs a noise-immune FS.
- Secondly, we currently have ECC RAM, but we have no way to protect data in non-ROM memory from SEU.
- Thirdly, it will be cheaper if we can use software solutions instead of hardware ones.

## Current state
| TODO List:                                             |Stat:|
|--------------------------------------------------------|-----|
| Base implementation.                                   | [V] |
| Complete porting from `CordellOS` project.             | [V] |
| Implement noise-immune algorithms.                     | [?] |
| Create format tool for creating modified FS on Flashs. | [.] |
| Testing and comparing with other FS.                   | [.] |
| Conference                                             | [.] |

# Theory summary
## Abstract
I had noticed that on the entire internet, there are no attempts to bring to life the [FAT32](https://wiki.osdev.org/FAT) file system for embedded or desktop systems. Usually, users and programmers simply shift to other file systems like [EXT4](https://wiki.osdev.org/Ext4) due to many technical issues in `FAT32`, but with this decision, they lose the simplicity of the first solution. And as we all know, for the reduction of errors, things should be made simpler. </br>
`NIFAT32` itself is a project that should work in embedded systems, like the [STM32F103C6T8](https://www.st.com/en/microcontrollers-microprocessors/stm32f103c6.html), or in home-made operating systems, like mine — [CordellOS](https://github.com/j1sk1ss/CordellOS.PETPRJ). For this purpose, the target file system should be lightweight and optimized for a single-threaded environment. </br>
Why `FAT32` as base? — Simple answer: because `FAT32` is a well-known file system with strong community support and a simple data structure. Also, `FAT32` has obvious parts where it can be upgraded to work in [SEU](https://en.wikipedia.org/wiki/Single-event_upset) and embedded contexts. I am talking about these aspects:
- The File Allocation Table, which contains the entire `FAT32` logic.
- `directory_entry_t`, which is the basis for representing files in the file system.
- Data flows from disk to high-level abstractions.

## Why SEU is dangerous?
Well, `SEU`, or single event upset, is a physical event when electrons in the `CPU`, `RAM`, etc., get disturbed by ionic particles. This is very dangerous when a program controls, for example, an entire plane or space station. That's why any program solution that will work in high-evaluated systems should be prepared for `SEU`. </br>
Of course, there are a couple of solutions for reducing the impact of `SEU` on chips, like:
- [ECC](https://community.fs.com/encyclopedia/ecc-memory.html)
But the problem here is that this solution works in `RAM` instead of `Flash` memory. This means that we can say that all data in `RAM` is secured, but we can't make a similar statement about `RWM` (read/write memory). (For `ROM`, the situation is different due to technical implementation. See this [topic](https://hackernoon.com/differences-between-ram-rom-and-flash-memory-all-you-need-to-know-ghr341i) for why `ROM` is secured against `SEU`).

## Impact of SEU

## File Allocation Table
As mentioned, the **File Allocation Table (FAT)** is the most important part of the `FAT32` file system. I won't explain how this table works — just check this [topic](https://en.wikipedia.org/wiki/Design_of_the_FAT_file_system). </br>
The original implementation of FAT simply saved the entire table in the first sectors of the disk. If we're talking about `Flash` memory — which is the cheapest solution in terms of storage capacity vs. price, according to [this article](https://nexusindustrialmemory.com/choosing-between-flash-and-eeprom-finding-the-perfect-memory-type-for-your-embedded-system/) — it comes with serious risks, especially in `embedded` systems. These risks are explained in more detail [here](https://en.wikipedia.org/wiki/Flash_memory).

The main issue is that `SEU` can disturb electrons in `Flash`, which may cause bit-flips that can silently corrupt data. If the `FAT` table is affected, the consequences can include:
- corrupted file allocation metadata,
- unreadable or wrongly linked files,
- complete file system failure.

Therefore, storing such critical structures in unprotected `Flash` without redundancy or error correction makes the system vulnerable to undetectable corruption.

## directory_entry_t
## Data-Flows

## References
- FAT32 carcase was taken from my OS project on [github](https://github.com/j1sk1ss/CordellOS.PETPRJ) 
- FS API itself was built with help of this [topic](https://wiki.osdev.org/FAT)
- SEU [topic](https://en.wikipedia.org/wiki/Single-event_upset)
- ECC [topic](https://community.fs.com/encyclopedia/ecc-memory.html)

