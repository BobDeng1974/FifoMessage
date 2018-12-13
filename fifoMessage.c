#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

/**************************************************** FIFO ************************************************************/

struct DataFIFO
{
	unsigned char *buf;
	int putP, getP, size, free;
};

void Fifo_init(struct DataFIFO *fifo, int size, unsigned char *buf)
{
	fifo->buf = buf;
	fifo->free = size;
	fifo->size = size;
	fifo->putP = 0;
	fifo->getP = 0;
	
	return;
}

int Fifo_putPut(struct DataFIFO *fifo, unsigned char data)
{
	if (fifo->free == 0) {
		return -1;
	}
	
	fifo->buf[fifo->putP] = data;
	fifo->putP++;
	if (fifo->putP == fifo->size) {
		fifo->putP = 0;
	}
	fifo->free--;
	
	return 0;
}

int Fifo_get(struct DataFIFO *fifo)
{
	unsigned char data;
	
	if (fifo->free == fifo->size) {
		return -1;
	}
	
	data = fifo->buf[fifo->getP];
	fifo->getP++;
	if (fifo->getP == fifo->size) {
		fifo->getP = 0;
	}
	fifo->free++;
	
	return data;
}

void Fifo_WriteBuffer(struct DataFIFO *fifo, unsigned char* buf, unsigned int len)
{
	while (len--) {
		Fifo_putPut(fifo, *buf++);
	}
}

void Fifo_ReadBuffer(struct DataFIFO *fifo, unsigned char* buf, unsigned int len)
{
	while (len--) {
		*buf++ = Fifo_get(fifo);
	}
}

void Fifo_ReadOnlyBuffer(struct DataFIFO *fifo, unsigned char* buf, unsigned int offset, unsigned int len)
{
	int getP = fifo->getP;
	
	while(offset--) {
		getP++;
		if (getP == fifo->size) {
			getP = 0;
		}
	}

	while (len--) {
		*buf++ = fifo->buf[getP];
		getP++;
		if (getP == fifo->size) {
			getP = 0;
		}
	}
}

int Fifo_status(struct DataFIFO *fifo)
{
	return fifo->size-fifo->free;
}

int Fifo_free(struct DataFIFO *fifo)
{
	return fifo->free;
}

/**************************************************** FIFO ************************************************************/

/************************************************ Message FIFO ********************************************************/

typedef struct
{
	struct DataFIFO					dataFifo;
	unsigned char						Front;
	unsigned char						Rear;
	unsigned char						ParkNum;
	unsigned char						ParkFree;
}MessageFifoTypeDef;

/**********************************************************************************************************
 @Function			void netMessageFifoInit(MessageFifoTypeDef *pMessageFifo, unsigned char *buf, int size, unsigned char parkNum)
 @Description			netMessageFifoInit				: 初始化消息队列
 @Input				pMessageFifo					: FiFo队列地址
					buf							: FiFo队列数据存储地址
					size							: FiFo队列数据存储大小
					parkNum						: 消息存储最大包数
 @Return				void
**********************************************************************************************************/
void netMessageFifoInit(MessageFifoTypeDef *pMessageFifo, unsigned char *buf, int size, unsigned char parkNum)
{
	Fifo_init(&pMessageFifo->dataFifo, size, buf);

	pMessageFifo->Front = 0;
	pMessageFifo->Rear = 0;
	pMessageFifo->ParkNum = parkNum;
	pMessageFifo->ParkFree = parkNum;
}

/**********************************************************************************************************
 @Function			bool netMessageFifoisFull(MessageFifoTypeDef *pMessageFifo, int writtenLen)
 @Description			netMessageFifoisFull			: 消息队列是否已满
 @Input				pMessageFifo					: FiFo队列地址
					writtenLen					: 将要写入FiFo数据大小
 @Return				true							: 已满
					false						: 未满
**********************************************************************************************************/
bool netMessageFifoisFull(MessageFifoTypeDef *pMessageFifo, int writtenLen)
{
	if ((writtenLen + 2) > Fifo_free(&pMessageFifo->dataFifo)) {
		return true;
	}

	if (((pMessageFifo->Rear + 1) % pMessageFifo->ParkNum) == pMessageFifo->Front) {
		return true;
	}

	return false;
}

/**********************************************************************************************************
 @Function			bool netMessageFifoisEmpty(MessageFifoTypeDef *pMessageFifo)
 @Description			netMessageFifoisEmpty			: 消息队列是否已空
 @Input				pMessageFifo					: FiFo队列地址
 @Return				true							: 已空
					false						: 未空
**********************************************************************************************************/
bool netMessageFifoisEmpty(MessageFifoTypeDef *pMessageFifo)
{
	if (Fifo_status(&pMessageFifo->dataFifo) == 0) {
		return true;
	}

	if (pMessageFifo->Front == pMessageFifo->Rear) {
		return true;
	}

	return false;
}

/**********************************************************************************************************
 @Function			bool netMessageFifoDiscard(MessageFifoTypeDef *pMessageFifo)
 @Description			netMessageFifoDiscard			: 丢弃最早一包消息队列数据
 @Input				pMessageFifo					: FiFo队列地址
 @Return				true							: 丢弃成功
					false						: 丢弃失败
**********************************************************************************************************/
bool netMessageFifoDiscard(MessageFifoTypeDef *pMessageFifo)
{
	unsigned char lenbuf[2];
	unsigned short len = 0;

	if (netMessageFifoisEmpty(pMessageFifo) == true) {
		return false;
	}

	/* step1 : 读取消息数据前2字节的数据长度 */
	Fifo_ReadBuffer(&pMessageFifo->dataFifo, lenbuf, 2);
	len = lenbuf[0];
	len |= lenbuf[1] << 8;

	/* step2 : 丢弃队列有效消息数据 */
	while (len--) {
		Fifo_get(&pMessageFifo->dataFifo);
	}

	/* step3 : 数据包处理 */
	pMessageFifo->Front = (pMessageFifo->Front + 1) % pMessageFifo->ParkNum;
	pMessageFifo->ParkFree++;

	return true;
}

/**********************************************************************************************************
 @Function			bool netMessageFifoEnqueue(MessageFifoTypeDef *pMessageFifo, unsigned char* buf, unsigned short len)
 @Description			netMessageFifoEnqueue			: 数据写入消息队列
 @Input				pMessageFifo					: FiFo队列地址
					buf							: 需写入数据地址
					len							: 需写入数据长度
 @Return				true							: 写入成功
					false						: 写入失败
**********************************************************************************************************/
bool netMessageFifoEnqueue(MessageFifoTypeDef *pMessageFifo, unsigned char* buf, unsigned short len)
{
	unsigned char lenbuf[2];

	if ((len + 2) > pMessageFifo->dataFifo.size) {
		return false;
	}

	/* step1 : 腾出需写入数据空间 */
	while (netMessageFifoisFull(pMessageFifo, len) == true) {
		netMessageFifoDiscard(pMessageFifo);
	}

	/* step2 : 写入消息数据前2字节的数据长度 */
	lenbuf[0] = len & 0xFF;
	lenbuf[1] = (len >> 8) & 0xFF;
	Fifo_WriteBuffer(&pMessageFifo->dataFifo, lenbuf, 2);

	/* step3 : 写入队列有效消息数据 */
	Fifo_WriteBuffer(&pMessageFifo->dataFifo, buf, len);

	/* step4 : 数据包处理 */
	pMessageFifo->Rear = (pMessageFifo->Rear + 1) % pMessageFifo->ParkNum;
	pMessageFifo->ParkFree--;

	return true;
}

/**********************************************************************************************************
 @Function			bool netMessageFifoDequeue(MessageFifoTypeDef *pMessageFifo, unsigned char* buf, unsigned short* len)
 @Description			netMessageFifoDequeue			: 数据读出消息队列
 @Input				pMessageFifo					: FiFo队列地址
					buf							: 需读出数据地址
					len							: 需读出数据长度地址
 @Return				true							: 读取成功
					false						: 读取失败
**********************************************************************************************************/
bool netMessageFifoDequeue(MessageFifoTypeDef *pMessageFifo, unsigned char* buf, unsigned short* len)
{
	unsigned char lenbuf[2];

	if (netMessageFifoisEmpty(pMessageFifo) == true) {
		return false;
	}

	/* step1 : 读取消息数据前2字节的数据长度 */
	Fifo_ReadOnlyBuffer(&pMessageFifo->dataFifo, lenbuf, 0, 2);
	*len = lenbuf[0];
	*len |= lenbuf[1] << 8;

	/* step2 : 读取队列有效消息数据 */
	Fifo_ReadOnlyBuffer(&pMessageFifo->dataFifo, buf, 2, *len);

	return true;
}

/**********************************************************************************************************
 @Function			unsigned char netMessageFifoFront(MessageFifoTypeDef *pMessageFifo)
 @Description			netMessageFifoFront				: 获取消息队列队头值
 @Input				pMessageFifo					: FiFo队列地址
 @Return				消息队列队头值
**********************************************************************************************************/
unsigned char netMessageFifoFront(MessageFifoTypeDef *pMessageFifo)
{
	return pMessageFifo->Front;
}

/**********************************************************************************************************
 @Function			unsigned char netMessageFifoRear(MessageFifoTypeDef *pMessageFifo)
 @Description			netMessageFifoRear				: 获取消息队列队尾值
 @Input				pMessageFifo					: FiFo队列地址
 @Return				消息队列队尾值
**********************************************************************************************************/
unsigned char netMessageFifoRear(MessageFifoTypeDef *pMessageFifo)
{
	return pMessageFifo->Rear;
}

/************************************************ Message FIFO ********************************************************/

#define	MESSAGEFIFO_PARKNUM_MAX			5
#define	MESSAGEFIFO_PARKSIZE_MAX			70

MessageFifoTypeDef						messageFifo;
unsigned char							messageBuf[MESSAGEFIFO_PARKSIZE_MAX];

/************************************************ Test Buffer *********************************************************/

unsigned char testFifoReadbuf[1024];
unsigned short testFifoReadlen;

unsigned char *testFifoWritebuf = "Test Fifo Message!";
unsigned short testFifoWritelen = 18;

/************************************************ Debug Buffer ********************************************************/

unsigned char templenbuf[2];
unsigned short templen = 0x5555;

/************************************************ Debug Buffer ********************************************************/

void DebugPrintfMessageFifo(void)
{
	printf("messageFifo->dataFifo->buf  : Addr = %d\r\n", messageFifo.dataFifo.buf);
	printf("messageFifo->dataFifo->size : val  = %d\r\n", messageFifo.dataFifo.size);
	printf("messageFifo->dataFifo->free : val  = %d\r\n", messageFifo.dataFifo.free);
	printf("messageFifo->dataFifo->putP : val  = %d\r\n", messageFifo.dataFifo.putP);
	printf("messageFifo->dataFifo->getP : val  = %d\r\n", messageFifo.dataFifo.getP);
	printf("messageFifo->ParkNum  : val  = %d\r\n", messageFifo.ParkNum);
	printf("messageFifo->ParkFree : val  = %d\r\n", messageFifo.ParkFree);
	printf("messageFifo->Front    : val  = %d\r\n", messageFifo.Front);
	printf("messageFifo->Rear     : val  = %d\r\n", messageFifo.Rear);
}

int main(int argc, char const *argv[])
{
	int ret = 0;

	printf("\r\nInitiative Parameter :\r\n");
	printf("messageBuf : Addr = %d\r\n", &messageBuf);
	printf("messageBuf : Size = %d\r\n", sizeof(messageBuf));
	printf("messageBuf : MaxPark = %d\r\n", MESSAGEFIFO_PARKNUM_MAX);

	printf("\r\nnetMessageFifoInit...\r\n");
	netMessageFifoInit(&messageFifo, messageBuf, sizeof(messageBuf), MESSAGEFIFO_PARKNUM_MAX);
	DebugPrintfMessageFifo();

	printf("\r\nnetMessageFifoEnqueue1...\r\n");
	ret = netMessageFifoEnqueue(&messageFifo, testFifoWritebuf, testFifoWritelen);
	if (ret != true) printf("MessageFifoEnqueue Fail\r\n");
	DebugPrintfMessageFifo();
	printf("EnqueueData : %s\r\n", testFifoWritebuf);
	printf("EnqueueLen : %d\r\n", testFifoWritelen);

	printf("\r\nnetMessageFifoEnqueue2...\r\n");
	ret = netMessageFifoEnqueue(&messageFifo, testFifoWritebuf, testFifoWritelen);
	if (ret != true) printf("MessageFifoEnqueue Fail\r\n");
	DebugPrintfMessageFifo();
	printf("EnqueueData : %s\r\n", testFifoWritebuf);
	printf("EnqueueLen : %d\r\n", testFifoWritelen);

	printf("\r\nnetMessageFifoEnqueue3...\r\n");
	ret = netMessageFifoEnqueue(&messageFifo, testFifoWritebuf, testFifoWritelen);
	if (ret != true) printf("MessageFifoEnqueue Fail\r\n");
	DebugPrintfMessageFifo();
	printf("EnqueueData : %s\r\n", testFifoWritebuf);
	printf("EnqueueLen : %d\r\n", testFifoWritelen);

	printf("\r\nnetMessageFifoEnqueue4...\r\n");
	ret = netMessageFifoEnqueue(&messageFifo, testFifoWritebuf, testFifoWritelen);
	if (ret != true) printf("MessageFifoEnqueue Fail\r\n");
	DebugPrintfMessageFifo();
	printf("EnqueueData : %s\r\n", testFifoWritebuf);
	printf("EnqueueLen : %d\r\n", testFifoWritelen);

	printf("\r\nnetMessageFifoEnqueue5...\r\n");
	ret = netMessageFifoEnqueue(&messageFifo, testFifoWritebuf, testFifoWritelen);
	if (ret != true) printf("MessageFifoEnqueue Fail\r\n");
	DebugPrintfMessageFifo();
	printf("EnqueueData : %s\r\n", testFifoWritebuf);
	printf("EnqueueLen : %d\r\n", testFifoWritelen);

	printf("\r\nnetMessageFifoEnqueue6...\r\n");
	ret = netMessageFifoEnqueue(&messageFifo, testFifoWritebuf, testFifoWritelen);
	if (ret != true) printf("MessageFifoEnqueue Fail\r\n");
	DebugPrintfMessageFifo();
	printf("EnqueueData : %s\r\n", testFifoWritebuf);
	printf("EnqueueLen : %d\r\n", testFifoWritelen);


	printf("\r\nnetMessageFifoDequeue...\r\n");
	ret = netMessageFifoDequeue(&messageFifo, testFifoReadbuf, &testFifoReadlen);
	if (ret != true) printf("MessageFifoDequeue Fail!\r\n");
	DebugPrintfMessageFifo();
	printf("DequeueData : %s\r\n", testFifoReadbuf);
	printf("DequeueLen : %d\r\n", testFifoReadlen);

	printf("\r\nnetMessageFifoDiscard...\r\n");
	ret = netMessageFifoDiscard(&messageFifo);
	if (ret != true) printf("MessageFifoDiscard Fail!\r\n");
	DebugPrintfMessageFifo();

	printf("\r\nnetMessageFifoFront : %d\r\n", netMessageFifoFront(&messageFifo));
	printf("\r\netMessageFifoRear   : %d\r\n", netMessageFifoRear(&messageFifo));

#if 0
	printf("\r\n\r\nDebug : \r\n");
	templenbuf[0] = 0x9A;
	templenbuf[1] = 0x4C;
	templen = templenbuf[0];
	templen |= templenbuf[1] << 8;
	printf("templen = %X\r\n", templen);
	printf("templen First = %X\r\n", templen & 0xFF);
	printf("templen Second = %X\r\n", (templen >> 8) &0xFF);
#endif

	printf("\r\n");
	
#if 1
	system("pause");
#endif
	
	return 0;
}

/************************************************** End File **********************************************************/
