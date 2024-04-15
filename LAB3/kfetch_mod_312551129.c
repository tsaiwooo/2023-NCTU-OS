#include <linux/atomic.h> 
#include <linux/cdev.h> 
#include <linux/delay.h> 
#include <linux/device.h> 
#include <linux/fs.h> 
#include <linux/init.h> 
#include <linux/kernel.h> /* for sprintf() */ 
#include <linux/module.h> 
#include <linux/printk.h> 
#include <linux/types.h> 
#include <linux/uaccess.h> /* for get_user and put_user */ 
#include <linux/version.h> 
#include <asm/errno.h> 
#include <linux/utsname.h> // get host name
#include <linux/cpumask.h>
#include <linux/seq_file.h>
#include <linux/processor.h>
#include <linux/string.h>
#include <linux/sysinfo.h> // memory
#include <linux/mm.h> // memory
#include <linux/mmzone.h>
#include <linux/sched.h>
#include <linux/jiffies.h>
#include <linux/timekeeping.h>



#define DEVICE_NAME "kfetch"
static int major;
static struct class *cls; 


enum { 
    CDEV_NOT_USED = 0, 
    CDEV_EXCLUSIVE_OPEN = 1, 
}; 

// flag for information
int cpu = 1;
int memory = 1;
int cores = 1;
int processes = 1;
int kernel_info = 1;
int running_time = 1;
int *flags_info[6] = {&kernel_info, &cores, &cpu, &memory, &running_time, &processes};





/* Is device open? Used to prevent multiple access to device */ 
static atomic_t already_open = ATOMIC_INIT(CDEV_NOT_USED); 


static int kfetch_open(struct inode *inode,struct file *file)
{
    if (atomic_cmpxchg(&already_open, CDEV_NOT_USED, CDEV_EXCLUSIVE_OPEN)) 
        return -EBUSY; 
 
    try_module_get(THIS_MODULE); 
 
    return 0; 
}

static int kfetch_release(struct inode *inode,struct file *file)
{
    atomic_set(&already_open, CDEV_NOT_USED); 

    /* Decrement the usage count, or else once you opened the file, you will 
     * never get rid of the module. 
     */ 
    module_put(THIS_MODULE); 
    return 0;
}

static ssize_t kfetch_read(struct file *filp,char __user *buffer,size_t length,loff_t *offset)
{
    char ans[6][128];
    char *kfetch_newbuf = NULL;
    char seperator[64];

    
    kfetch_newbuf = kmalloc(1024, GFP_KERNEL); 

    /* space fill */
    struct new_utsname *kernel_utsname;
    kernel_utsname = &init_uts_ns.name;


    /* seperator*/
    memset(seperator,'-',strlen(kernel_utsname->nodename));
    seperator[strlen(kernel_utsname->nodename)] = '\0';



    int ans_idx = 0;
    char temp_buf[64];
    for(int idx=0 ; idx<6 ; idx++){
        if(*flags_info[idx]==1){
            switch(idx){
                case 0:
                    sprintf(temp_buf,"Kernel:\t%s",kernel_utsname->release);
                    strcpy(ans[ans_idx],temp_buf);
                    ans_idx++;
                    break;
                case 2:
                    struct cpuinfo_x86 *c = &cpu_data(0);
                    sprintf(temp_buf,"CPU:\t%s",c->x86_model_id);
                    strcpy(ans[ans_idx],temp_buf);
                    ans_idx++;
                    break;
                case 1:
                    unsigned int cpu_online_count = num_online_cpus();
                    unsigned int cpu_total_count = num_present_cpus();
                    sprintf(temp_buf,"CPUs:\t%d / %d",cpu_online_count,cpu_total_count);
                    strcpy(ans[ans_idx],temp_buf);
                    ans_idx++;
                    break;
                case 3:
                    struct sysinfo i;
                    si_meminfo(&i);
                    sprintf(temp_buf,"Mem:\t%ld MB / %ld MB",(i.totalram - i.freeram)*i.mem_unit/(1024*1024),i.totalram * i.mem_unit / (1024*1024));
                    strcpy(ans[ans_idx],temp_buf);
                    ans_idx++;
                    break;
                case 4:
                    struct timespec64 ts;
                    ktime_get_boottime_ts64(&ts);
                    unsigned long uptime = ts.tv_sec/60;// mins
                    // unsigned long uptime = jiffies / (HZ*60);
                    sprintf(temp_buf,"Uptime:\t%ld mins",uptime);
                    strcpy(ans[ans_idx],temp_buf);
                    ans_idx++;
                    break;
                case 5:
                    struct task_struct *task;
                    int thread_count = 0;
                    for_each_process(task){
                        thread_count++;
                    }

                    sprintf(temp_buf,"Procs:\t%d",thread_count);
                    strcpy(ans[ans_idx],temp_buf);
                    ans_idx++;
                    break;
                default:
                    break;
            }
        }
    
    }

    if(ans_idx<6){
        for(;ans_idx<6;ans_idx++){
            strcpy(ans[ans_idx],"  ");
        }
    }

        sprintf(kfetch_newbuf,\
"\
                       \t%s\n\
            .-.        \t%s\n\
           (.. |       \t%s\n\
           <>  |       \t%s\n\
          / --- \\      \t%s\n\
         ( |   | |     \t%s\n\
       |\\_)___/\\)/\\   \t%s\n\
      <__)------(__/   \t%s\n"\
,kernel_utsname->nodename,seperator,ans[0],ans[1],ans[2],ans[3],ans[4],ans[5]);

    
    pr_info("%s\n",kfetch_newbuf);

    if (copy_to_user(buffer, kfetch_newbuf, 1024)) {
        pr_alert("Failed to copy data to user");
        return 0;
    }

    return 1024;
}

static ssize_t kfetch_write(struct file *filp,const char __user *buffer,size_t length,loff_t *offset)
{
    int mask_info;
    if (copy_from_user(&mask_info, buffer, length)) {
        pr_alert("Failed to copy data from user");
        return 0;
    }

    char binary[10];
    int num = mask_info;
    for(int i=5 ; i>=0 ; i--){
        binary[5-i] = (num & 1) + '0';
        num >>= 1;
    }
    
    // set flag
    if(binary[0]=='1'){
        kernel_info = 1;
    }else if(mask_info!=0){
        kernel_info = 0;
    }

    if(binary[1]=='1'){
        cores = 1;
    }else if(mask_info!=0){
        cores = 0;
    }

    if(binary[2]=='1'){
        cpu = 1;
    }else if(mask_info!=0){
        cpu = 0;
    }

    if(binary[3]=='1'){
        memory = 1;
    }else if(mask_info!=0){
        memory = 0;
    }

    if(binary[4]=='1'){
        running_time = 1;
    }else if(mask_info!=0){
        running_time = 0;
    }

    if(binary[5]=='1'){
        processes = 1;
    }else if(mask_info!=0){
        processes = 0;
    }


    return 0;

}

const static struct file_operations kfetch_ops = {
    .owner   = THIS_MODULE,
    .read    = kfetch_read,
    .write   = kfetch_write,
    .open    = kfetch_open,
    .release = kfetch_release,
};


static int os_init_module(void) 
{ 
    pr_info("Insert success\n"); 
    
    major = register_chrdev(0,DEVICE_NAME,&kfetch_ops);

    if(major<0){
        pr_alert("Register char device failed with %d\n",major);
        return major;
    }

    pr_info("assigned major number %d\n",major);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0) 
    cls = class_create(DEVICE_NAME); 
#else 
    cls = class_create(THIS_MODULE, DEVICE_NAME); 
#endif 

    device_create(cls, NULL, MKDEV(major, 0), NULL, DEVICE_NAME); 
    pr_info("Device created in /dev/%s\n",DEVICE_NAME);

 
    /* A non 0 return means init_module failed; module can't be loaded. */ 
    return 0; 
}

static void os_cleanup_module(void) 
{ 
    device_destroy(cls, MKDEV(major, 0)); 
    class_destroy(cls); 
    
    unregister_chrdev(major, DEVICE_NAME); 
    pr_info("unload success\n"); 
} 


module_init(os_init_module);
module_exit(os_cleanup_module);

 
MODULE_LICENSE("GPL");
