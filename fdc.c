/* Floppy Disk Controlelr Driver.
 *
 * The following code was ported from FDCDriver.{h,cpp}
 * of the MONA operating system (Revision 5332).
 * http://www.monaos.org
 *
 * This code could run only on Qemu.
 */

#define FDC_DMA_BUF_ADDR   0x80000
#define FDC_DMA_BUF_SIZE  512

#define FDC_DOR_PRIMARY   0x3f2
#define FDC_MSR_PRIMARY   0x3f4
#define FDC_DR_PRIMARY    0x3f5
#define FDC_CCR_PRIMARY   0x3f7

#define FDC_DMA_S_SMR     0x0a
#define FDC_DMA_S_MR      0x0b
#define FDC_DMA_S_CBP     0x0c
#define FDC_DMA_S_BASE    0x04
#define FDC_DMA_S_COUNT   0x05
#define FDC_DMA_PAGE2     0x81

#define FDC_MOTA_START    0x10
#define FDC_DMA_ENABLE    0x08
#define FDC_REST_ENABLE   0x04
#define FDC_DR_SELECT_A   0x00

#define FDC_START_MOTOR (FDC_DMA_ENABLE | FDC_MOTA_START | FDC_REST_ENABLE | FDC_DR_SELECT_A)
#define FDC_STOP_MOTOR  (FDC_DMA_ENABLE | FDC_REST_ENABLE | FDC_DR_SELECT_A)

#define FDC_COMMAND_SPECIFY 0x03
#define FDC_COMMAND_READ    0xe6
#define FDC_COMMAND_WRITE   0xc5

int fdc_initialize(void);
int fdc_read(int cylinder, int head, int sector);
int fdc_read2(void);
int fdc_write(int cylinder, int head, int sector);
int fdc_write2(void);

static void fdc_send_command(char* command, int length);
static void fdc_start_dma(void);
static void fdc_stop_dma(void);
static void fdc_setup_dma_read(char* buf, int size);
static void fdc_setup_dma_write(char* buf, int size);
static void fdc_motor(int on);
static void fdc_send_command(char* command, int length);
static char fdc_get_result();
static void fdc_wait_status(char mask, char status);
static void fdc_delay(int c);

extern int in8(int port);
extern int out8(int port, int value);


int fdc_initialize(void) {
  char cmd[3];
  cmd[0] = FDC_COMMAND_SPECIFY;
  cmd[1] = 0xC1; /* SRT=4ms, HUT=16ms */
  cmd[2] = 0x10; /* HLT=16ms DMA */

  out8(0xda, 0x00);
  fdc_delay(1);
  out8(0x0d, 0x00);
  fdc_delay(1);
  out8(0xd0, 0x00);
  fdc_delay(1);
  out8(0x08, 0x00);
  fdc_delay(1);
  out8(0xd6, 0xc0);
  fdc_delay(1);
  out8(0x0b, 0x46);
  fdc_delay(1);
  out8(0xd4, 0x00);
  fdc_delay(1);

  out8(FDC_CCR_PRIMARY, 0);
  fdc_motor(1);  /* on */
  fdc_send_command(cmd, sizeof(cmd));
  fdc_motor(0);  /* off */
  return 0;
}

/* cylinder: 0..
   head:  0 or 1
   sector: 1..18

   If the sector is the n-th logical sector, then it is:
     cylinder = n / (18 * 2),
     head = (n / 18) % 2,
     sector = 1 + n % 18.
 */
int fdc_read(int cylinder, int head, int sector) {
  char cmd[9];
  cmd[0] = FDC_COMMAND_READ;
  cmd[1] = (char)((head & 1) << 2);
  cmd[2] = (char)cylinder;
  cmd[3] = (char)head;
  cmd[4] = (char)sector;
  cmd[5] = 0x02;
  cmd[6] = 0x12;
  cmd[7] = 0x1b;
  cmd[8] = 0xff;

  /* seek(cylinder); */

  fdc_setup_dma_read((char*)FDC_DMA_BUF_ADDR, FDC_DMA_BUF_SIZE);

  fdc_send_command(cmd, sizeof(cmd));
  return 0;
}

int fdc_read2(void) {
  char results[7];
  int i;
  for (i = 0; i < 7; i++)
    results[i] = fdc_get_result();

  fdc_stop_dma();
  return results[0] & 0xc0 == 0;  /* 0 if error */
}

/* cylinder: 0..
   head:  0 or 1
   sector: 1..18

   If the sector is the n-th logical sector, then it is:
     cylinder = n / (18 * 2),
     head = (n / 18) % 2,
     sector = 1 + n % 18.
 */
int fdc_write(int cylinder, int head, int sector) {
  char cmd[9];
  cmd[0] = FDC_COMMAND_WRITE;
  cmd[1] = (char)((head & 1) << 2);
  cmd[2] = (char)cylinder;
  cmd[3] = (char)head;
  cmd[4] = (char)sector;
  cmd[5] = (char)0x02;
  cmd[6] = (char)0x12;
  cmd[7] = (char)0x1b;
  cmd[8] = (char)0x00;

  fdc_setup_dma_write((char*)FDC_DMA_BUF_ADDR, FDC_DMA_BUF_SIZE);

  /* seek(cylinder); */

  fdc_send_command(cmd, sizeof(cmd));
  return 0;
}

int fdc_write2(void) {
  int i;
  char results[7];

  fdc_stop_dma();
  for (i = 0; i < 7; i++)
    results[i] = fdc_get_result();

  return results[0] & 0xc0 == 0;  /* 0 if error */
}

static void fdc_start_dma(void) {
  /* mask channel 2 */
  out8(FDC_DMA_S_SMR, 0x02);
}

static void fdc_stop_dma(void) {
  /* unmask channel2 */
  out8(FDC_DMA_S_SMR, 0x06);
}

static void fdc_setup_dma_read(char* buffer, int size) {
  int buf = (int)buffer;
  size--;

  fdc_stop_dma();

  /* write */
  out8(FDC_DMA_S_MR, 0x46);

  out8(FDC_DMA_S_CBP, 0);
  out8(FDC_DMA_S_BASE,  buf & 0xff);
  out8(FDC_DMA_S_BASE,  (buf >> 8) & 0xff);
  out8(FDC_DMA_S_COUNT, size & 0xff);
  out8(FDC_DMA_S_COUNT, (size >>8) & 0xff);
  out8(FDC_DMA_PAGE2,   (buf >>16) & 0xff);

  fdc_start_dma();
}

static void fdc_setup_dma_write(char* buffer, int size) {
  int buf = (int)buffer;
  size--;

  fdc_stop_dma();

  /* read */
  out8(FDC_DMA_S_MR, 0x4a);

  out8(FDC_DMA_S_CBP, 0);
  out8(FDC_DMA_S_BASE, buf & 0xff);
  out8(FDC_DMA_S_BASE, (buf >> 8) & 0xff);
  out8(FDC_DMA_S_COUNT, size & 0xff);
  out8(FDC_DMA_S_COUNT, (size >> 8) & 0xff);
  out8(FDC_DMA_PAGE2  , (buf >>16) & 0xff);

  fdc_start_dma();
}

static void fdc_motor(int on) {
  if (on) {
    out8(FDC_DOR_PRIMARY, FDC_START_MOTOR);
    fdc_delay(4);
  }
  else
    out8(FDC_DOR_PRIMARY, FDC_STOP_MOTOR);
}

static void fdc_send_command(char* command, int length) {
  int i;
  for (i = 0; i < length; i++) {
    fdc_wait_status(0x80 | 0x40, 0x80);
    out8(FDC_DR_PRIMARY, command[i]);
  }
}

static char fdc_get_result() {
  fdc_wait_status(0xd0, 0xd0);
  return in8(FDC_DR_PRIMARY);
}

static void fdc_wait_status(char mask, char status) {
  char current;
  do {
    current = in8(FDC_MSR_PRIMARY);
  } while ((current & mask) != status);
}

static void fdc_delay(int c) {
  while (c-- > 0)
    in8(0x80);
}
