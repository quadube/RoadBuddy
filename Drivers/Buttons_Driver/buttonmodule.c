#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/module.h>
#include <asm/io.h>
#include <linux/timer.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/pid.h>
#include <linux/sched/signal.h>
#include <asm/siginfo.h>
#include <linux/rcupdate.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/io.h>
#include <linux/gpio.h>     //GPIO
#include <linux/interrupt.h>

MODULE_LICENSE("GPL");
MODULE_INFO(intree, "Y");

//DEBOUNCE STUFF
#define EN_DEBOUNCE
 
#ifdef EN_DEBOUNCE
#include <linux/jiffies.h> 
extern unsigned long volatile jiffies;
unsigned long old_jiffie = 0;
#endif

//DEVICE STUFF
#define DEVICE_NAME "button0"
#define CLASS_NAME "buttonClass"
#define SIG_BUZZBUT (SIGRTMIN+7)
#define SIG_PROBUT (SIGRTMIN+6)

struct class* buttonDevice_class = NULL;
static dev_t buttonDevice_majorminor;	
static struct cdev c_dev;  		// Character device structure

//GPIO STUFF
static const int GPIO_PIN_PRO = 22;	// button
static const int GPIO_PIN_BUZ = 10;	// button

// SIGNAL STUFF
pid_t pid; 						//process id
struct pid *pid_struct;			//pointer to process id struct 
struct task_struct *task; 		//pointer to task struct
struct siginfo info_pro; 			//signal struct
struct siginfo info_buzz;

//interrupts
unsigned int GPIO_irqNumber_pro, GPIO_irqNumber_buzz;

//INTERRUPT HANDLER PROBUT
static irqreturn_t IRQ_HandlerPROBUT(int irq,void *dev_id) 
{
  	static unsigned long flags = 0;
	
	
	//Debounce
  	#ifdef EN_DEBOUNCE
   	unsigned long diff = jiffies - old_jiffie;
   	if (diff < 20)
   	{
     		return IRQ_HANDLED;
   	}
  	old_jiffie = jiffies;
  	#endif
	printk(KERN_INFO "Interrupt handler PROBUT");

   	local_irq_save(flags); //disable all hard interrupts at CPU level

  	//Send a signal to User Space
	if (task != NULL) {
		if(send_sig_info(SIG_PROBUT, &info_pro, task) < 0){
			printk(KERN_INFO "Unable to send signal\n");
		}
	}

    local_irq_restore(flags); //restore interrupts at CPU level
  	return IRQ_HANDLED;
}

//INTERRUPT HANDLER BUZZBUT
static irqreturn_t IRQ_HandlerBUZZBUT(int irq,void *dev_id) 
{
  	static unsigned long flags = 0;
	
	
	//Debounce
  	#ifdef EN_DEBOUNCE
   	unsigned long diff = jiffies - old_jiffie;
   	if (diff < 20)
   	{
     		return IRQ_HANDLED;
   	}
  	old_jiffie = jiffies;
  	#endif
	printk(KERN_INFO "Interrupt handler BUZZBUT");

   	local_irq_save(flags); //disable all hard interrupts at CPU level

  	//Send a signal to User Space
	if (task != NULL) {
		if(send_sig_info(SIG_BUZZBUT, &info_buzz, task) < 0){
			printk(KERN_INFO "Unable to send signal\n");
		}
	}

    local_irq_restore(flags); //restore interrupts at CPU level
  	return IRQ_HANDLED;
}

//CLOSE
int button_close(struct inode *p_inode, struct file * pfile){
	task=NULL;						//free task pointer
	pfile->private_data = NULL;
	return 0;
}

//OPEN
int button_open(struct inode* p_indode, struct file *pfile){

	//GET CALLING TASK TO KNOW WHERE TO SEND THE SIGNAL
	pid = current->pid;										//get pid from calling task			
	pid_struct = find_get_pid(pid);							//get the struct pid
	if((task = pid_task(pid_struct,PIDTYPE_PID)) == NULL){	//get the calling task struct from the pid struct
		printk("Cannon find PID from user program\r\n");
		return 0;
	}
	
	//SIGNAL PROBUT
	memset(&info_pro, 0, sizeof(struct siginfo));		//allocate memory for the signal struct
	info_pro.si_signo = SIG_PROBUT;						//which signal 
	info_pro.si_code = SI_QUEUE;						


	//SIGNAL BUZZBUT
	memset(&info_buzz, 0, sizeof(struct siginfo));		//allocate memory for the signal struct
	info_buzz.si_signo = SIG_BUZZBUT;					//which signal 
	info_buzz.si_code = SI_QUEUE;						

	//Get the IRQ number for GPIO
  	GPIO_irqNumber_pro = gpio_to_irq(GPIO_PIN_PRO);
	pr_info("IRQ number PRO = %d\n", GPIO_irqNumber_pro);
	GPIO_irqNumber_buzz = gpio_to_irq(GPIO_PIN_BUZ);
  	pr_info("IRQ number BUZZ = %d\n", GPIO_irqNumber_buzz);


  	//Register the Interrupt
	// (irq number, handler, interrupt rising/falling, name, shared interrupt number)
  	if (request_irq(GPIO_irqNumber_pro, (void *)IRQ_HandlerPROBUT, IRQF_TRIGGER_RISING, "pro_button", NULL)) 
      	pr_err("PROBUT: cannot register IRQ ");
	
  	if (request_irq(GPIO_irqNumber_buzz, (void *)IRQ_HandlerBUZZBUT, IRQF_TRIGGER_RISING, "buzz_button", NULL)) 
      	pr_err("BUZZBUT: cannot register IRQ ");

	return 0;
}

//file operations structure
static struct file_operations buttonDevice_fops = {
	.owner = THIS_MODULE,
	.release = button_close,
	.open = button_open,
};

//init
static int __init buttonModule_init(void) {
	int ret;
	struct device *dev_ret;

	pr_alert("%s: called\n",__FUNCTION__);

	if ((ret = alloc_chrdev_region(&buttonDevice_majorminor, 0, 1, DEVICE_NAME)) < 0) {
		return ret;
	}

	if (IS_ERR(buttonDevice_class = class_create(THIS_MODULE, CLASS_NAME))) {
		unregister_chrdev_region(buttonDevice_majorminor, 1);
		return PTR_ERR(buttonDevice_class);
	}
	if (IS_ERR(dev_ret = device_create(buttonDevice_class, NULL, buttonDevice_majorminor, NULL, DEVICE_NAME))) {
		class_destroy(buttonDevice_class);
		unregister_chrdev_region(buttonDevice_majorminor, 1);
		return PTR_ERR(dev_ret);
	}

	cdev_init(&c_dev, &buttonDevice_fops);
	c_dev.owner = THIS_MODULE;
	if ((ret = cdev_add(&c_dev, buttonDevice_majorminor, 1)) < 0) {
		printk(KERN_NOTICE "Error %d adding device", ret);
		device_destroy(buttonDevice_class, buttonDevice_majorminor);
		class_destroy(buttonDevice_class);
		unregister_chrdev_region(buttonDevice_majorminor, 1);
		return ret;
	}
	return 0;
}

//exit
static void __exit buttonModule_exit(void) {
	free_irq(GPIO_irqNumber_pro,NULL);	//free the use of the interrupt
	free_irq(GPIO_irqNumber_buzz,NULL);	//free the use of the interrupt
	cdev_del(&c_dev);
	device_destroy(buttonDevice_class, buttonDevice_majorminor);
	class_destroy(buttonDevice_class);
	unregister_chrdev_region(buttonDevice_majorminor, 1);
}

module_init(buttonModule_init);
module_exit(buttonModule_exit);
