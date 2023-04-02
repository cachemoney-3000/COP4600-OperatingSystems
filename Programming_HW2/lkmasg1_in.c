/**
 * File:	lkmasg1_input.c
 * Adapted for Linux 5.15 by: Joshua Samontanez
 * Class:	COP4600-SP23
 */
#include <linux/init.h>
#include <linux/module.h>	  // Core header for modules.
#include <linux/device.h>	  // Supports driver model.
#include <linux/kernel.h>	  // Kernel header for convenient functions.
#include <linux/fs.h>		  // File-system support.
#include <linux/uaccess.h>	  // User access copy function support.
#include <linux/mutex.h>
#include <linux/vmalloc.h>


#define DEVICE_NAME "lkmasg1_in" // Device name.
#define CLASS_NAME "char_in"	  ///< The device class -- this is a character device driver
#define MAX_SIZE 1024		  // Maximum size of buffer 

MODULE_LICENSE("GPL");						 	///< The license type -- this affects available functionality
MODULE_AUTHOR("Joshua Samontanez");				///< The author -- visible when you use modinfo
MODULE_DESCRIPTION("lkmasg1_input Kernel Module"); 	///< The description -- see modinfo
MODULE_VERSION("0.1");						 	///< A version number to inform users

DEFINE_MUTEX(input_mutex);
EXPORT_SYMBOL(input_mutex);

/**
 * Important variables that store data and keep track of relevant information.
 */
static int major_number;
static short message_size;
static int open_counter = 0;

struct shared_data {
    char message[MAX_SIZE];
    short message_size; 
};

struct shared_data *shared_memory;
EXPORT_SYMBOL(shared_memory);

static struct class *lkmasg1_input_class = NULL;	///< The device-driver class struct pointer
static struct device *lkmasg1_input_device = NULL; ///< The device-driver device struct pointer


/**
 * Prototype functions for file operations.
 */
static int open(struct inode *, struct file *);
static int close(struct inode *, struct file *);
static ssize_t write(struct file *, const char *, size_t, loff_t *);


/**
 * File operations structure and the functions it points to.
 */
static struct file_operations fops =
{
	.owner = THIS_MODULE,
	.open = open,
	.release = close,
	.write = write,
};


/**
 * Initializes module at installation
 */
static int __init initialize(void)
{
	printk(KERN_INFO "lkmasg1_input: Installing module.\n");
	printk(KERN_INFO "lkmasg1_input: Waiting for the lock.\n");
	
	// Allocate a major number for the device.
	major_number = register_chrdev(0, DEVICE_NAME, &fops);
	if (major_number < 0)
	{
		printk(KERN_ALERT "lkmasg1_input could not register number.\n");
		return major_number;
	}

	printk(KERN_INFO "lkmasg1_input: Registered correctly with major number %d\n", major_number);

	// Register the device class
	lkmasg1_input_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(lkmasg1_input_class))
	{ // Check for error and clean up if there is
		unregister_chrdev(major_number, DEVICE_NAME);
		printk(KERN_ALERT "Failed to register device class\n");
		return PTR_ERR(lkmasg1_input_class); // Correct way to return an error on a pointer
	}

	printk(KERN_INFO "lkmasg1_input: Device class registered correctly\n");

	// Register the device driver
	lkmasg1_input_device = device_create(lkmasg1_input_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
	if (IS_ERR(lkmasg1_input_device))
	{								 // Clean up if there is an error
		class_destroy(lkmasg1_input_class); // Repeated code but the alternative is goto statements
		unregister_chrdev(major_number, DEVICE_NAME);
		printk(KERN_ALERT "lkmasg1_input: Failed to create the device\n");
		return PTR_ERR(lkmasg1_input_device);
	}


	mutex_init(&input_mutex);       /// Initialize the mutex lock dynamically at runtime
	printk(KERN_INFO "lkmasg1_input: Lock acquired\n");
	
	// Allocate the shared memory
	shared_memory = (struct shared_data *)vmalloc(sizeof(struct shared_data));
	if (!shared_memory) {
		printk(KERN_ERR "lkmasg1_input: Failed to allocate shared memory\n");
		return -ENOMEM;
	}

	// Initialize the shared memory
	memset(shared_memory, 0, sizeof(struct shared_data));


	printk(KERN_INFO "lkmasg1_input: Device class created correctly\n"); // Made it! device was initialized

	return 0;
}


/*
 * Removes module, sends appropriate message to kernel
 */
static void __exit clear(void)
{
	vfree(shared_memory);
	shared_memory = NULL;
	printk(KERN_INFO "lkmasg1_input: Shared memory free\n");
	
	
    mutex_destroy(&input_mutex);        /// destroy the dynamically-allocated mutex
    printk(KERN_INFO "lkmasg1_input: Mutex lock destroyed.\n");

	printk(KERN_INFO "lkmasg1_input: Removing module.\n");
	device_destroy(lkmasg1_input_class, MKDEV(major_number, 0)); // remove the device
	class_unregister(lkmasg1_input_class);						  // unregister the device class
	class_destroy(lkmasg1_input_class);						  // remove the device class
	unregister_chrdev(major_number, DEVICE_NAME);		  // unregister the major number
	printk(KERN_INFO "lkmasg1_input: Goodbye from the LKM!\n");
	unregister_chrdev(major_number, DEVICE_NAME);

	return;
}


/*
 * Opens device module, sends appropriate message to kernel
 */
static int open(struct inode *inodep, struct file *filep)
{
	open_counter++;
	printk(KERN_INFO "lkmasg1_input: Device has been opened %d time(s)\n", open_counter);
	try_module_get(THIS_MODULE);
	return 0;
}


/*
 * Closes device module, sends appropriate message to kernel
 */
static int close(struct inode *inodep, struct file *filep)
{
	printk(KERN_INFO "lkmasg1_input: Device closed.\n");
	
	/* 
	 * Decrement the usage count, or else once you opened the file, you'll
	 * never get get rid of the module. 
	 */
	module_put(THIS_MODULE);
	return 0;
}


/*
 * Writes to the device
 */
static ssize_t write(struct file *filep, const char *buffer, size_t len, loff_t *offset)
{	
	if (len >= MAX_SIZE) {
		printk(KERN_ALERT "lkmasg1_input: The length (%zu) exceeds the max size of the buffer.");
		printk(KERN_INFO "lkmasg1_input:  The message has been reduced.\n", len);
		len = MAX_SIZE - 1; // Reduce the length to fit within the MAX_SIZE, subtract 1 since it's null terminated
	}

	// Create a temporary buffer to hold the message and its length
	char tmp_message[MAX_SIZE];
	int tmp_len = snprintf(tmp_message, sizeof(tmp_message), "%s", buffer);

	if (tmp_len >= MAX_SIZE) {
		// The formatted message is too long, truncate it to store only up to the amount available
		tmp_message[MAX_SIZE - 1] = '\0';
		tmp_len = MAX_SIZE - 1;
	}
	
	
	// Copy the formatted message to the shared message buffer
	if(!mutex_trylock(&input_mutex)){ // Try to acquire the mutex (i.e., put the lock on/down)
		                        // returns 1 if successful and 0 if there is contention
		printk(KERN_ALERT "lkmasg1_input: Device in use by another process");
		return -EBUSY;
	}
	
	printk(KERN_ALERT "lkmasg1_input: Critical section");
	
	// Clear the shared memory before overwriting it with the new message
	memset(shared_memory, 0, sizeof(struct shared_data));
	// Copy the formatted message to the shared message buffer
	shared_memory->message_size = snprintf(shared_memory->message, MAX_SIZE, "%s", tmp_message);
	shared_memory->message[shared_memory->message_size] = '\0'; // Ensure the string is null-terminated


	mutex_unlock(&input_mutex); // Release the mutex (i.e., put the lock off/up)
	
	printk(KERN_ALERT "lkmasg1_input: Lock was released");
	printk(KERN_INFO "lkmasg1_input: Received %zu bytes from the user\n", len);
	
	return len;
}


module_init(initialize);
module_exit(clear); 	





