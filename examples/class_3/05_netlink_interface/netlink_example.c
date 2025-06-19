#include <linux/module.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>
#include <net/sock.h>

#define NETLINK_COURSE_PROTOCOL 31  // Custom protocol number
#define MSG_TYPE_HELLO 1
#define MSG_TYPE_COUNTER 2
#define MSG_TYPE_STATUS 3

struct course_message {
    int msg_type;
    int data;
    char text[64];
};

static struct sock *nl_sock = NULL;
static int message_counter = 0;

// Send message to user space
static void send_message_to_user(int pid, int msg_type, int data, const char *text)
{
    struct sk_buff *skb;
    struct nlmsghdr *nlh;
    struct course_message *msg;
    int msg_size = sizeof(struct course_message);
    
    // Allocate socket buffer
    skb = nlmsg_new(msg_size, GFP_KERNEL);
    if (!skb) {
        pr_err("Failed to allocate socket buffer\n");
        return;
    }
    
    // Add netlink header
    nlh = nlmsg_put(skb, 0, 0, NLMSG_DONE, msg_size, 0);
    if (!nlh) {
        pr_err("Failed to add netlink header\n");
        kfree_skb(skb);
        return;
    }
    
    // Add our message
    msg = (struct course_message *)nlmsg_data(nlh);
    msg->msg_type = msg_type;
    msg->data = data;
    strncpy(msg->text, text, sizeof(msg->text) - 1);
    msg->text[sizeof(msg->text) - 1] = '\0';
    
    // Send to user space
    if (netlink_unicast(nl_sock, skb, pid, MSG_DONTWAIT) < 0) {
        pr_err("Failed to send message to user space\n");
    } else {
        pr_info("Sent message to PID %d: type=%d, data=%d, text='%s'\n",
                pid, msg_type, data, text);
    }
}

// Handle incoming messages from user space
static void netlink_recv_msg(struct sk_buff *skb)
{
    struct nlmsghdr *nlh;
    struct course_message *msg;
    int pid;
    
    nlh = (struct nlmsghdr *)skb->data;
    pid = nlh->nlmsg_pid;
    msg = (struct course_message *)nlmsg_data(nlh);
    
    pr_info("Received from PID %d: type=%d, data=%d, text='%s'\n",
            pid, msg->msg_type, msg->data, msg->text);
    
    // Process different message types
    switch (msg->msg_type) {
    case MSG_TYPE_HELLO:
        message_counter++;
        send_message_to_user(pid, MSG_TYPE_HELLO, message_counter, 
                           "Hello from kernel!");
        break;
        
    case MSG_TYPE_COUNTER:
        send_message_to_user(pid, MSG_TYPE_COUNTER, message_counter,
                           "Current counter value");
        break;
        
    case MSG_TYPE_STATUS:
        send_message_to_user(pid, MSG_TYPE_STATUS, 0,
                           "Kernel module is running");
        break;
        
    default:
        pr_warn("Unknown message type: %d\n", msg->msg_type);
        send_message_to_user(pid, 0, -1, "Unknown command");
        break;
    }
}

static int __init netlink_example_init(void)
{
    struct netlink_kernel_cfg cfg = {
        .input = netlink_recv_msg,
    };
    
    // Create netlink socket
    nl_sock = netlink_kernel_create(&init_net, NETLINK_COURSE_PROTOCOL, &cfg);
    if (!nl_sock) {
        pr_err("Failed to create netlink socket\n");
        return -ENOMEM;
    }
    
    pr_info("Netlink example module loaded\n");
    pr_info("Protocol: %d\n", NETLINK_COURSE_PROTOCOL);
    pr_info("Message types: HELLO=%d, COUNTER=%d, STATUS=%d\n",
            MSG_TYPE_HELLO, MSG_TYPE_COUNTER, MSG_TYPE_STATUS);
    
    return 0;
}

static void __exit netlink_example_exit(void)
{
    if (nl_sock) {
        netlink_kernel_release(nl_sock);
    }
    pr_info("Netlink example module unloaded\n");
}

module_init(netlink_example_init);
module_exit(netlink_example_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kernel Course");
MODULE_DESCRIPTION("Netlink socket communication example");