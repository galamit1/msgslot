//
// Created by galam on 01/12/2021.
//

#include "message_slot.h"

#undef __KERNEL__
#define __KERNEL__
#undef MODULE
#define MODULE


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/slab.h>

MODULE_LICENSE("GPL");
#define MAJOR 240

#include "message_slot.h"

//--------------------data structures functions----------------------------------

static MessageSlot *messages_slots[MAX_MESSAGES_SLOTS + 1] = {NULL};

MessageSlot * create_message_slot(void ) {
    MessageSlot * message_slot = kmalloc(sizeof(MessageSlot), GFP_KERNEL);
    if (message_slot == NULL) {
        return NULL;
    }
    message_slot->head = NULL;
    return message_slot;
}

Channel * create_channel(unsigned int channel_id) {
    Channel * channel = kmalloc(sizeof(Channel), GFP_KERNEL);
    if (channel == NULL) {
        return NULL;
    }
    channel->message = kmalloc(sizeof(char) * MAX_MESSAGE_LENGTH, GFP_KERNEL);
    if (channel->message == NULL) {
        kfree(channel);
        return NULL;
    }
    channel->channel_id = channel_id;
    channel->length = 0;
    channel->next = NULL;
    return channel;
}

Channel * get_channel_from_message_slot(MessageSlot * message_slot, unsigned int channel_id) {
    Channel * cur_channel;
    cur_channel = message_slot->head;
    while (cur_channel != NULL) {
        if (cur_channel->channel_id == channel_id) {
            return cur_channel;
        }
        if (cur_channel->next != NULL) {
            cur_channel = (Channel *)cur_channel->next;
        } else {
            return NULL;
        }
    }
    return NULL;
}

void add_channel_to_message_slot(MessageSlot * message_slot, Channel * channel) {
    channel->next = message_slot->head;
    message_slot->head = channel;
}

void free_message_slot(MessageSlot * message_slot) {
    Channel * cur_channel;
    Channel * next_channel;
    if (message_slot == NULL) {
        return;
    }
    cur_channel = message_slot->head;
    while (cur_channel != NULL) {
        next_channel = cur_channel->next;
        printk("freeing channel: %d\n", cur_channel->channel_id);
        if (cur_channel->message != NULL) {
            kfree(cur_channel->message);
        }
        cur_channel->message = NULL;
        cur_channel->channel_id = 0;
        kfree(cur_channel);
        cur_channel = next_channel;
    }
    kfree(message_slot);
}

//---------------------device functions-------------------------------------

static int device_open(struct inode *inode, struct file *file) {
    int minor;
    minor = iminor(inode);
    if (messages_slots[minor] == NULL) {
        messages_slots[minor] = create_message_slot();
        if (messages_slots[minor] == NULL) {
            printk(KERN_ERR "Failed to malloc space for message slot with minor: %d", minor);
            return ERROR;
        }
    }
    return SUCCESS;
}


static long device_ioctl(struct file *file, unsigned int ioctl_command_id, unsigned long ioctl_param) {
    if (ioctl_command_id == MSG_SLOT_CHANNEL) {
        file->private_data = (void *) ioctl_param; // set the file descriptor's channel id
        return SUCCESS;
    } else {
        return -EINVAL;
    }
}


static ssize_t device_write(struct file *file, const char __user *buffer, size_t length, loff_t *offset) {
    int minor, status;
    Channel * channel;
    char * message;
    unsigned int channel_id;

    minor = iminor(file->f_inode);
    if (messages_slots[minor] == NULL || file->private_data == NULL || buffer == NULL) {
        printk(KERN_ERR "Got wrong arguments from user\n");
        return -EINVAL;
    }
    channel_id = (unsigned int)file->private_data;
    if (length <= 0 || length > MAX_MESSAGE_LENGTH) {
        printk(KERN_ERR "Wrong message length: %ld\n", length);
        return -EMSGSIZE;
    }
    channel = get_channel_from_message_slot(messages_slots[minor], channel_id);
    if (channel == NULL) {
        printk("create_channel");
        channel = create_channel(channel_id);
        add_channel_to_message_slot(messages_slots[minor], channel);
        if (channel == NULL) {
            printk(KERN_ERR "Error creating new channel\n", minor);
            return -ENOSPC;
        }
    }
    if (channel->message == NULL) {
        printk(KERN_ERR "Error allocating msg\n");
        return -ENOSPC;
    }
    message = kmalloc(sizeof(char) * length, GFP_KERNEL);
    if (message == NULL) {
        printk(KERN_ERR "Error allocating msg\n");
        return -ENOSPC;
    }
    status = copy_from_user(message, buffer, length);
    if (status != 0){
        printk(KERN_ERR "Error in copy_from_user function\n");
        return -ENOSPC;
    }
    channel->length = length;
    memcpy(channel->message, message, length);
    kfree(message);
    return length;
}


static ssize_t device_read(struct file *file, char __user *buffer, size_t length, loff_t *offset) {
    int minor, status;
    Channel * channel;
    char * message;
    unsigned int channel_id;

    minor = iminor(file->f_inode);
    if (messages_slots[minor] == NULL || file->private_data == NULL || buffer == NULL) {
        printk(KERN_ERR "Got wrong arguments from user\n");
        return -EINVAL;
    }
    channel_id = (unsigned int)file->private_data;
    printk("minor %d, channel %d", minor, channel_id);
    channel = get_channel_from_message_slot(messages_slots[minor], channel_id);
    if (channel == NULL || channel->length == 0) {
        printk(KERN_ERR "No channel has been set with channel_id: %d\n", channel_id);
        return -EWOULDBLOCK;
    }
    if (length < channel->length || buffer == NULL) {
        printk(KERN_ERR "The provided length is too small to hold the msg: %ld\n", length);
        return -ENOSPC;
    }
    message = kmalloc(sizeof(char) * channel->length, GFP_KERNEL);
    if (message == NULL) {
        printk(KERN_ERR "Error allocating msg\n");
        return -ENOSPC;
    }
    memcpy(message, channel->message, channel->length);
    status = copy_to_user(buffer, message, channel->length);
    if (status != 0){
        printk(KERN_ERR "Error in copy_to_user function\n");
        kfree(message);
        return -ENOSPC;
    }
    kfree(message);
    return channel->length;
}

//-----------------------------init and remove device----------------------------------
struct file_operations Fops = {
        .owner      = THIS_MODULE,
        .read           = device_read,
        .write          = device_write,
        .open           = device_open,
        .unlocked_ioctl          = device_ioctl,
};

static int __init message_slot_init(void) {
    int status;
    printk("Register device to major: %d\n", MAJOR);
    status = register_chrdev(MAJOR, DEVICE_NAME, &Fops);
    if (status < 0) {
        printk(KERN_ERR "Failed to register major %d: status: %d\n", MAJOR, status);
        return ERROR;
    }
    return SUCCESS;
}

static void __exit message_slot_exit(void) {
    int i;
    for (i=0; i<MAX_MESSAGES_SLOTS + 1; i++) {
        if (messages_slots[i] != NULL) {
            printk("Free message_slot i=%d\n", i);
            free_message_slot(messages_slots[i]);
        }
    }
    unregister_chrdev(MAJOR, DEVICE_NAME);
    printk("Successfully unregistered device\n");
}


module_init(message_slot_init);
module_exit(message_slot_exit);
