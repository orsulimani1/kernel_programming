#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/netlink.h>

#define NETLINK_COURSE_PROTOCOL 31
#define MSG_TYPE_HELLO 1
#define MSG_TYPE_COUNTER 2  
#define MSG_TYPE_STATUS 3

struct course_message {
   int msg_type;
   int data;
   char text[64];
};

int main()
{
   int sock_fd;
   struct sockaddr_nl src_addr, dest_addr;
   struct nlmsghdr *nlh = NULL;
   struct course_message *msg;
   struct iovec iov;
   struct msghdr msg_hdr;
   
   // Create socket
   sock_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_COURSE_PROTOCOL);
   if (sock_fd < 0) {
       perror("socket");
       return -1;
   }
   
   // Setup source address
   memset(&src_addr, 0, sizeof(src_addr));
   src_addr.nl_family = AF_NETLINK;
   src_addr.nl_pid = getpid();
   
   if (bind(sock_fd, (struct sockaddr *)&src_addr, sizeof(src_addr)) < 0) {
       perror("bind");
       close(sock_fd);
       return -1;
   }
   
   // Setup destination (kernel)
   memset(&dest_addr, 0, sizeof(dest_addr));
   dest_addr.nl_family = AF_NETLINK;
   dest_addr.nl_pid = 0;  // Kernel
   dest_addr.nl_groups = 0;
   
   // Allocate message
   nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(sizeof(struct course_message)));
   memset(nlh, 0, NLMSG_SPACE(sizeof(struct course_message)));
   nlh->nlmsg_len = NLMSG_SPACE(sizeof(struct course_message));
   nlh->nlmsg_pid = getpid();
   nlh->nlmsg_flags = 0;
   
   // Setup message header (reused for all messages)
   iov.iov_base = (void *)nlh;
   iov.iov_len = nlh->nlmsg_len;
   msg_hdr.msg_name = (void *)&dest_addr;
   msg_hdr.msg_namelen = sizeof(dest_addr);
   msg_hdr.msg_iov = &iov;
   msg_hdr.msg_iovlen = 1;
   
   printf("Testing netlink communication...\n");
   
   // Test HELLO message
   msg = (struct course_message *)NLMSG_DATA(nlh);
   msg->msg_type = MSG_TYPE_HELLO;
   msg->data = 123;
   strcpy(msg->text, "Hello from user space!");
   
   printf("Sending HELLO message...\n");
   sendmsg(sock_fd, &msg_hdr, 0);
   
   // Receive response
   memset(nlh, 0, NLMSG_SPACE(sizeof(struct course_message)));
   recvmsg(sock_fd, &msg_hdr, 0);
   msg = (struct course_message *)NLMSG_DATA(nlh);
   printf("Received: type=%d, data=%d, text='%s'\n", 
          msg->msg_type, msg->data, msg->text);
   
   // Test COUNTER message  
   nlh->nlmsg_len = NLMSG_SPACE(sizeof(struct course_message));
   nlh->nlmsg_pid = getpid();  // Fix: reset PID
   msg->msg_type = MSG_TYPE_COUNTER;
   msg->data = 0;
   strcpy(msg->text, "Get counter");
   
   printf("Sending COUNTER request...\n");
   sendmsg(sock_fd, &msg_hdr, 0);
   
   memset(nlh, 0, NLMSG_SPACE(sizeof(struct course_message)));
   recvmsg(sock_fd, &msg_hdr, 0);
   msg = (struct course_message *)NLMSG_DATA(nlh);
   printf("Received: type=%d, data=%d, text='%s'\n",
          msg->msg_type, msg->data, msg->text);
   
   // Test STATUS message
   nlh->nlmsg_len = NLMSG_SPACE(sizeof(struct course_message));
   nlh->nlmsg_pid = getpid();  // Fix: reset PID
   msg->msg_type = MSG_TYPE_STATUS;
   msg->data = 0;
   strcpy(msg->text, "Status request");
   
   printf("Sending STATUS request...\n");
   sendmsg(sock_fd, &msg_hdr, 0);
   
   memset(nlh, 0, NLMSG_SPACE(sizeof(struct course_message)));
   recvmsg(sock_fd, &msg_hdr, 0);
   msg = (struct course_message *)NLMSG_DATA(nlh);
   printf("Received: type=%d, data=%d, text='%s'\n",
          msg->msg_type, msg->data, msg->text);
   
   close(sock_fd);
   free(nlh);
   
   printf("Netlink test completed!\n");
   return 0;
}