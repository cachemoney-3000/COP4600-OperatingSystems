/**
 * File:	pa2_out.c
 * Adapted for Linux 5.15 by: Joshua Samontanez
 * Class:	COP4600-SP23
 */
#include <linux/init.h>
#include <linux/module.h>	  // Core header for modules.
#include <linux/device.h>	  // Supports driver model.
#include <linux/kernel.h>	  // Kernel header for convenient functions.
#include <linux/fs.h>		  // File-system support.
#include <linux/uaccess.h>	  // User access copy function support.
#include <linux/vmalloc.h>
#include <linux/mutex.h>

#define DEVICE_NAME "pa2_out" // Device name.
#define CLASS_NAME "char_out"	  ///< The device class -- this is a character device driver
#define MAX_SIZE 1024		  // Maximum size of buffer 

MODULE_LICENSE("GPL");						 	///< The license type -- this affects available functionality
MODULE_AUTHOR("Joshua Samontanez");				///< The author -- visible when you use modinfo
MODULE_DESCRIPTION("lkmasg1_output Kernel Module"); 	///< The description -- see modinfo
MODULE_VERSION("0.1");						 	///< A version number to inform users

/**
 * Important variables that store data and keep track of relevant information.
 */
static int major_number;
static int open_counter = 0;


static struct class *lkmasg1_output_class = NULL;	///< The device-driver class struct pointer
static struct device *lkmasg1_output_device = NULL; ///< The device-driver device struct pointer

struct shared_data {
    char message[MAX_SIZE];
    short message_size; 
};
extern struct shared_data *shared_memory;
extern struct mutex input_mutex;

/**
 * Prototype functions for file operations.
 */
static int open(struct inode *, struct file *);
static int close(struct inode *, struct file *);
static ssize_t read(struct file *, char *, size_t, loff_t *);


/**
 * File operations structure and the functions it points to.
 */
static struct file_operations fops =
{
	.owner = THIS_MODULE,
	.open = open,
	.release = close,
	.read = read,
};


/**
 * Initializes module at installation
 */
static int __init initialize(void)
{
	printk(KERN_INFO "lkmasg1_output: Installing module.\n");

	// Allocate a major number for the device.
	major_number = register_chrdev(0, DEVICE_NAME, &fops);
	if (major_number < 0)
	{
		printk(KERN_ALERT "lkmasg1_output could not register number.\n");
		return major_number;
	}

	printk(KERN_INFO "lkmasg1_output: Registered correctly with major number %d\n", major_number);

	// Register the device class
	lkmasg1_output_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(lkmasg1_output_class))
	{ // Check for error and clean up if there is
		unregister_chrdev(major_number, DEVICE_NAME);
		printk(KERN_ALERT "Failed to register device class\n");
		return PTR_ERR(lkmasg1_output_class); // Correct way to return an error on a pointer
	}

	printk(KERN_INFO "lkmasg1_output: Device class registered correctly\n");

	// Register the device driver
	lkmasg1_output_device = device_create(lkmasg1_output_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
	if (IS_ERR(lkmasg1_output_device))
	{								 // Clean up if there is an error
		class_destroy(lkmasg1_output_class); // Repeated code but the alternative is goto statements
		unregister_chrdev(major_number, DEVICE_NAME);
		printk(KERN_ALERT "lkmasg1_output: Failed to create the device\n");
		return PTR_ERR(lkmasg1_output_device);
	}


	printk(KERN_INFO "lkmasg1_output: Device class created correctly\n"); // Made it! device was initialized

	return 0;
}


/*
 * Removes module, sends appropriate message to kernel
 */
static void __exit clear(void)
{
	printk(KERN_INFO "lkmasg1_output: Removing module.\n");
	device_destroy(lkmasg1_output_class, MKDEV(major_number, 0)); // remove the device
	class_unregister(lkmasg1_output_class);						  // unregister the device class
	class_destroy(lkmasg1_output_class);						  // remove the device class
	unregister_chrdev(major_number, DEVICE_NAME);		  // unregister the major number
	printk(KERN_INFO "lkmasg1_output: Goodbye from the LKM!\n");
	unregister_chrdev(major_number, DEVICE_NAME);

	return;
}


/*
 * Opens device module, sends appropriate message to kernel
 */
static int open(struct inode *inodep, struct file *filep)
{
	open_counter++;
	printk(KERN_INFO "lkmasg1_output: Device has been opened %d time(s)\n", open_counter);
	try_module_get(THIS_MODULE);
	return 0;
}


/*
 * Closes device module, sends appropriate message to kernel
 */
static int close(struct inode *inodep, struct file *filep)
{
	printk(KERN_INFO "lkmasg1_output: Device closed.\n");
	
	/* 
	 * Decrement the usage count, or else once you opened the file, you'll
	 * never get get rid of the module. 
	 */
	module_put(THIS_MODULE);
	return 0;
}


/*
 * Reads from device, displays in userspace, and deletes the read data
 */
static ssize_t read(struct file *filep, char *buffer, size_t len, loff_t *offset)
{
    int error_count = 0;

    if (*offset >= MAX_SIZE) // Check if we have reached the end of the message
        return 0;

    if (shared_memory == NULL) {
        printk(KERN_INFO "lkmasg1_output: There is no message to read\n");
        return -EFAULT;
    }
	
	// Copy the formatted message to the shared message buffer
	if(!mutex_trylock(&input_mutex)){ // Try to acquire the mutex (i.e., put the lock on/down)
		                        // returns 1 if successful and 0 if there is contention
		printk(KERN_ALERT "lkmasg1_output: Device in use by another process");
		return -EBUSY;
	}
	printk(KERN_ALERT "lkmasg1_input: Critical section");
	
    // Copy message from shared memory to user buffer
    error_count = copy_to_user(buffer, shared_memory->message + *offset, shared_memory->message_size);
	
	// Clear the shared memory before overwriting it with the new message
	memset(shared_memory, 0, sizeof(struct shared_data));
	// Reset the message buffer to all null characters
	memset(shared_memory->message, 0, MAX_SIZE);
	// Reset the message size to 0
	shared_memory->message_size = 0;
	
	mutex_unlock(&input_mutex); // Release the mutex lock
    
	if (error_count == 0) {
		printk(KERN_INFO "lkmasg1_output: Read %lu bytes from shared memory\n", len);
		*offset += len;
		return len;
	} 
	else {
		printk(KERN_INFO "lkmasg1_output: Failed to read %lu bytes from shared memory\n", len);
		return -EFAULT;
	}  
}


module_init(initialize);
module_exit(clear); 	





