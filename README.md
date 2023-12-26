## dsync
dsync is a tool to sync source directories or files to a destination directory
based on size and modification time. I have a few directories where I keep adding
stuff and I just need to sync them to an external drive from time to time. In the
spirit of "write your own tools", I wrote my own.

## Usage
Running `dsync -h` prints the below usage information.
```
Usage: dsync [OPTION]... SOURCE... DIRECTORY
Sync/copy SOURCE(s) to DIRECTORY.

  -f       force copy SOURCE(s) to DIRECTORY even if they are in sync
  -j [N]   run N (max 255) threads that sync/copy source files

By default (without the -f option), dsync will copy SOURCE(s) to DIRECTORY only
if the files' size and modification time don't match (even if file in destination
is newer than the corresponding source file). If SOURCE(s) themselves are symbolic
links they will be resolved to their actual paths. dsync always preserves mode and
timestamps. Multiple threads can be used to sync/copy using the -j option which
can reduce total time in case of source directories with a lot of directories
and a lot of small files in them. dsync always recursively syncs/copies all the
contents of the given sources. Symbolic links inside SOURCE(s) are not followed
but copied themselves. Extra directories or files in destination directory are
not detected or deleted. dsync doesn't make sure data is written to disk.
```
By default, dsync uses one thread for traversing the sources and one thread
for doing the sync/copy work.

**Note:** Use of multiple threads has been provided as an option (-j) as using
multiple threads will not always improve the time needed specially when copying.
It can depend on a lot of factors like sources' size distribution, filesystem,
SSD/Hard disk etc etc.

## Implementation
dsync can use multiple threads (specified via the -j option) to do the sync/copy
work. The main thread traverses the given sources and adds the files that need
to be synced/copied to a **bounded multi-producer multi-consumer queue**. The
sync/copy threads dequeue entries from the queue and do the sync/copy work concurrently.
In linux (and freebsd), dsync tries to utilize the **copy_file_range** api if possible
falling back to traditional read write loop for copying. In other systems, the
read write loop is used. No output in terminal would mean that everything went
successfully. In case of errors, error messages are written to stderr and the error
messages might be interleaved as there is no synchronization when writing to stderr
from multiple threads.

## How to build
You will need gcc and make to build dsync. Follow the below steps.
1. clone this repository
2. cd dsync
3. make

One way to check the correctness of the tool would be to use the diff utility to
check if original source directory and corresponding destination directory contain
the same subdirectories and files. `diff -r original_dir dir_copied_using_dsync`.

## Benchmark
For comparison I have used GNU cp (9.1) and dsync comparing the total time for
them to copy linux-6.5.13 source repository. dsync can perform worse or better
depending on a lot of factors (see the note in the usage section). dsync performs
well with multiple threads when source contains directories with a lot of small
files. linux-6.5.13 source contains 5278 directories and 81053 files (1.4GB).

**Test machine details:** Test machine has an Intel i3-5005U CPU with 2 cores (4 threads)
with 16GiB RAM and a 500GB HP SSD S700. It is running debian 12 with linux kernel
6.1.0-17-amd64 and ext4 filesystem.

The linux-6.5.13 directory has been copied to an empty directory in the same
filesystem in every test run. And the measurements have been done using the
/usr/bin/time -v command. To reduce noise, all the tests have been run after
turning the machine off and on again and the values have been averaged over 3
runs (sample size is low really). For dsync, number of threads in the parantheses
mean the total number of threads that do the copy work (i.e., N means actually
total N+1 threads are running).

**cp command:**    `/usr/bin/time -v cp -R --preserve=mode,timestamps /linux-6.5.13 /empty`<br />
**dsync command:** `/usr/bin/time -v dsync -jN /linux-6.5.13 /empty`<br />
where N is 1,2,4,6 (N == 1 is equivalent to not providing any j option)

| Command (No of threads) | Elapsed (wall clock) time (seconds) | Maximum resident set size (kbytes) |
|-|-|-|
| cp | 20.18 | 2576 |
| dsync (1) | 21.82 | 2798.67 |
| dsync (2) | 13.75 | 2878.67 |
| dsync (4) | 8.59 | 2878.67 |
| dsync (6) | 7.60 | 2874.67 |

## Known limitations
* No windows support.
* Compilation will fail on older linux systems where copy_file_range is not available.
* Although the code is portable (I believe it should compile on traditional
bsd systems as well), dsync has not been compiled/tested on systems other than linux.
* Hard links to the same file are synced/copied multiple times.
