/**
 * File:	lkmasg1.c
 * Adapted for Linux 5.15 by: Joshua Samontanez
 * Class:	COP4600-SP23
 */


#include <linux/init.h>
#include <linux/module.h>	  // Core header for modules.
#include <linux/device.h>	  // Supports driver model.
#include <linux/kernel.h>	  // Kernel header for convenient functions.
#include <linux/fs.h>		  // File-system support.
#include <linux/uaccess.h>	  // User access copy function support.
#define DEVICE_NAME "lkmasg1" // Device name.
#define CLASS_NAME "char"	  ///< The device class -- this is a character device driver
#define MAX_SIZE 1024		  // Maximum size of buffer 

MODULE_LICENSE("GPL");						 	///< The license type -- this affects available functionality
MODULE_AUTHOR("Joshua Samontanez");				///< The author -- visible when you use modinfo
MODULE_DESCRIPTION("lkmasg1 Kernel Module"); 	///< The description -- see modinfo
MODULE_VERSION("0.1");						 	///< A version number to inform users


/**
 * Important variables that store data and keep track of relevant information.
 */
static int major_number;
static char message[MAX_SIZE] = {0};
static char *msg_Ptr;
static short message_size;
static int open_counter = 0;

static struct class *lkmasg1Class = NULL;	///< The device-driver class struct pointer
static struct device *lkmasg1Device = NULL; ///< The device-driver device struct pointer


/**
 * Prototype functions for file operations.
 */
static int open(struct inode *, struct file *);
static int close(struct inode *, struct file *);
static ssize_t read(struct file *, char *, size_t, loff_t *);
static ssize_t write(struct file *, const char *, size_t, loff_t *);


/**
 * File operations structure and the functions it points to.
 */
static struct file_operations fops =
{
	.owner = THIS_MODULE,
	.open = open,
	.release = close,
	.read = read,
	.write = write,
};


/**
 * Initializes module at installation
 */
static int __init initialize(void)
{
	printk(KERN_INFO "lkmasg1: Installing module.\n");

	// Allocate a major number for the device.
	major_number = register_chrdev(0, DEVICE_NAME, &fops);
	if (major_number < 0)
	{
		printk(KERN_ALERT "lkmasg1 could not register number.\n");
		return major_number;
	}

	printk(KERN_INFO "lkmasg1: Registered correctly with major number %d\n", major_number);

	// Register the device class
	lkmasg1Class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(lkmasg1Class))
	{ // Check for error and clean up if there is
		unregister_chrdev(major_number, DEVICE_NAME);
		printk(KERN_ALERT "Failed to register device class\n");
		return PTR_ERR(lkmasg1Class); // Correct way to return an error on a pointer
	}

	printk(KERN_INFO "lkmasg1: Device class registered correctly\n");

	// Register the device driver
	lkmasg1Device = device_create(lkmasg1Class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
	if (IS_ERR(lkmasg1Device))
	{								 // Clean up if there is an error
		class_destroy(lkmasg1Class); // Repeated code but the alternative is goto statements
		unregister_chrdev(major_number, DEVICE_NAME);
		printk(KERN_ALERT "Failed to create the device\n");
		return PTR_ERR(lkmasg1Device);
	}

	printk(KERN_INFO "lkmasg1: Device class created correctly\n"); // Made it! device was initialized

	return 0;
}


/*
 * Removes module, sends appropriate message to kernel
 */
static void __exit clear(void)
{
	printk(KERN_INFO "lkmasg1: Removing module.\n");
	device_destroy(lkmasg1Class, MKDEV(major_number, 0)); // remove the device
	class_unregister(lkmasg1Class);						  // unregister the device class
	class_destroy(lkmasg1Class);						  // remove the device class
	unregister_chrdev(major_number, DEVICE_NAME);		  // unregister the major number
	printk(KERN_INFO "lkmasg1: Goodbye from the LKM!\n");
	unregister_chrdev(major_number, DEVICE_NAME);

	return;
}


/*
 * Opens device module, sends appropriate message to kernel
 */
static int open(struct inode *inodep, struct file *filep)
{
	open_counter++;
	printk(KERN_INFO "lkmasg1: Device has been opened %d time(s)\n", open_counter);
	try_module_get(THIS_MODULE);
	return 0;
}


/*
 * Closes device module, sends appropriate message to kernel
 */
static int close(struct inode *inodep, struct file *filep)
{
	printk(KERN_INFO "lkmasg1: Device closed.\n");
	
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
	int bytes_read = 0;	// Store the number of bytes read

	if(*msg_Ptr == 0){
		printk(KERN_INFO "lkmasg1: There is no message to read.\n");
		return 0; // if there is no message
	}
	while(len && *msg_Ptr) {
		put_user(*(msg_Ptr++), buffer++);	// Read the message character by character (FIFO)

		// Update the counters
		len--;
		bytes_read++;
	}
	return bytes_read;
}


/*
 * Writes to the device
 */
static ssize_t write(struct file *filep, const char *buffer, size_t len, loff_t *offset)
{
	if(len >= MAX_SIZE){
		printk(KERN_INFO "lkmasg1: The length (%zu) exceeds the max size of the buffer, the message has been reduced.\n", len);
		len = MAX_SIZE - 1;	// Reduce the length to fit within the MAX_SIZE, subtract 1 since its null terminated
	}
	
	// Create a temporary buffer to hold the message and its length
	char tmp_message[MAX_SIZE];
	int tmp_len = snprintf(tmp_message, sizeof(tmp_message), "%s", buffer);

	if (tmp_len >= MAX_SIZE) {
		// The formatted message is too long, truncate it to store only up to the amount available
		tmp_message[MAX_SIZE - 1] = '\0';
		tmp_len = MAX_SIZE - 1;
    	}

	// Copy the formatted message to the message buffer
	strncpy(message, tmp_message, tmp_len);
	message[tmp_len] = '\0';

	message_size = strlen(message);  // Store the length of the stored message
	msg_Ptr = message;  // Pointer to the starting address of the message

	printk(KERN_INFO "lkmasg1: Received %zu characters from the user\n", len);
	return len;
}


module_init(initialize);
module_exit(clear);


