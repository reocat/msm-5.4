//struct rpmsg_ns_msg {         
//	char name[RPMSG_NAME_SIZE];           
//	u32 addr; 
//	u32 flags;       
//} __packed;

/* The feature bitmap for virtio rpmsg */
#define VIRTIO_RPMSG_F_NS	0 /* RP supports name service notifications */

/**
 * struct rpmsg_hdr - common header for all rpmsg messages
* @src: source address
   + * @dst: destination address
   + * @reserved: reserved for future use
   + * @len: length of payload (in bytes)
   + * @flags: message flags
   + * @data: @len bytes of message payload data
   + *
   + * Every message sent(/received) on the rpmsg bus begins with this header.
*/
struct rpmsg_hdr {
	u32 src;
	u32 dst;
	u32 reserved;
	u16 len;
	u16 flags;
	u8 data[0];
} __packed;

/**
	   + * struct rpmsg_ns_msg - dynamic name service announcement message
	   + * @name: name of remote service that is published
	   + * @addr: address of remote service that is published
	   + * @flags: indicates whether service is created or destroyed
	   + *
	   + * This message is sent across to publish a new service, or announce
	   + * about its removal. When we receive these messages, an appropriate
	   + * rpmsg channel (i.e device) is created/destroyed. In turn, the ->probe()
	   + * or ->remove() handler of the appropriate rpmsg driver will be invoked
	   + * (if/as-soon-as one is registered).
	   + */
struct rpmsg_ns_msg {
	char name[RPMSG_NAME_SIZE];
	u32 addr;
	u32 flags;
} __packed;

/**
		   + * enum rpmsg_ns_flags - dynamic name service announcement flags
		   + *
		   + * @RPMSG_NS_CREATE: a new remote service was just created
		   + * @RPMSG_NS_DESTROY: a known remote service was just destroyed
*/
enum rpmsg_ns_flags {
	RPMSG_NS_CREATE		= 0,
	RPMSG_NS_DESTROY	= 1,
};


