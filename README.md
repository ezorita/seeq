## Seeq: DNA/RNA pattern matching algorithm
---
## Contents: ##
    1. What is seeq?
    2. Source file list.
    3. Compilation and installation.
    4. Running seeq.
    5. Using seeq as a sequence trimmer.
    6. License.
    7. References.

---
## I. What is seeq?   ##

Seeq is a DNA/RNA pattern matching algorithm. Sequence matching is performed
based on a Levenshtein distance metric [1]. Typically, a file containing DNA
sequences is passed as input along with a DNA pattern. Seeq will search for
lines containing the matching pattern. By default, Seeq returns the matching
lines through the stadard output. Seeq has many other applications such as sequence
extraction, sequence trimming, etc.

II. Source file list
--------------------

* **main-seeq.c**        Seeq main file (parameter parsing).
* **seeq.c**             Main seeq algorithm.
* **seeq.h**             Main seeq algorithm header file.
* **Makefile**           Make instruction file.


III. Compilation and installation
---------------------------------

To install seeq you first need to clone or manually download the 
repository content from [github](http://github.com/ezorita/seeq):

 > git clone git://github.com/ezorita/seeq.git

the files should be downloaded in a folder named 'seeq'. To compile
just change the directory to 'seeq' and run make (Mac users require
'xcode', available at the Mac Appstore):

 > cd seeq

 > make

a binary file 'seeq' will be created. You can optionally make a
symbolic link to execute seeq from any directory:

 > sudo ln -s ./seeq /usr/bin/seeq


IV. Running seeq
----------------

Seeq runs on Linux and Mac. It has not been tested on Windows.

List of arguments:

  > seeq -[c | i | mnpsfeb] -[lhvz] PATTERN [INPUT_FILE]

  **PATTERN**
  
     The pattern must be a DNA/RNA sequence. Allowed characters are 'A', 'C',
     'G', 'T', 'U' and 'N', both lower and uppercase. Multiple characters
     (bases) will account as a match in the same position if so is specified
     using square brackets, i.e. the pattern 'A[CG]T' defines both 'C' and 'G'
     as valid matches for the 2nd nucleotide. Hence, the pattern will match both
     'ACT' and 'AGT' with distance 0. The 'unknown' character 'N' is equivalent
     to '[ACGTU]' and will match any character without any distance penalty.

  **INPUT_FILE**

     Optional parameter. Standard input is used when no input file is specified.
     The input file must contain DNA/RNA sequences separated with newline
     characters '\n'. Lines containing characters other than 'A', 'C', 'G',
     'T', 'U' or 'N' will be ignored. This allows direct use of FASTA or
     FASTQ files. Note, however, that tags and quality scores will not be
     present in the output.

  **-v or --version**

     Prints the software version and exits.
  
  **-z or --verbose**

     Verbose. Prints verbose information to the standard error channel.

  **-d or --distance** *distance*

     Defines the maximum Levenshtein distance for pattern matching.
     Default is 0.

  **-c or --count** *threads*

     Returns only the count of matching lines. When specified, all other
     options are ignored.

  **-i or --invert**

     Returns the non-matching lines. When specified, all other options,
     except 'lines' and 'count', are ignored.

  **-m or --match-only**

     Returns only the matched part of the matched lines.

  **-n or --no-printline**

     Does not print the matched line. If this option is set, additional
     format options must be specified.

  **-l or --lines**

     Prints the matched line number.

  **-p or --positions**

     Prints the position of the match in the matched line with the format
     [start index]-[end index].

  **-s or --print-dist**

     Prints the levenshtein distance between the match and the pattern.

  **-e or --end**

     Prints only the last part of the matched lines, starting after (not including)
     the matched part.

  **-b or --prefix**

     Prints only the beginning of the matched lines, ending before (not including)
     the matched part.

  **-f or --compact**

     Uses compact output format. Each matched line will produce an output as
     folllows:
     
     [line number]:[start index]-[end index]:[distance]

     When specified, other format options [mnlpseb] are ignored.


  **-h or --help**

     Prints usage information.


V. Using seeq as a sequence trimmer
-----------------------------------

Many high-throughput technologies use reference sequences both at the beginning
and the end of the sequence of interest. When the target sequence is not long enough,
the reference sequences may be partially present in the final read. Seeq may be used 
as a sequence trimmer to solve this problem. Consider an input file with the following
format:

    [constant prefix][DNA sequence][constant suffix]

The target DNA sequence may be extracted selecting several nucleotides from both the
prefix and the suffix and piping seeq in --prefix and --end modes:

    ./seeq --end [constant prefix] input_file.fasta | ./seeq --prefix [constant suffix]

Additionally, different distance thresholds may be specified to match the reference prefix
and suffix using '-d #'.

VI. License
-----------

Seeq is licensed under the GNU General Public License, version 3
(GPLv3), for more information read the LICENSE file or refer to:

  http://www.gnu.org/licenses/


VII. References
---------------

[1] Levenshtein, V. (1966), 'Binary Codes Capable of Correcting Deletions,
    Insertions and Reversals', Soviet Physics Doklady 10, 707.
