/* CXPHYS.SYS: read-only physical memory reader for CLXP3D on Windows XP. MIT, 2026.
 * One IOCTL: copy up to 64 KB of physical memory (RAM, frame buffer or MMIO) into the caller's buffer through MmMapIoSpace.
 * Nothing is ever written to the mapped memory. CLXP3D installs it as a demand-start service, uses it, then removes it. */
#include <ntddk.h>
#define CX_IOCTL_READ CTL_CODE(FILE_DEVICE_UNKNOWN,0x801,METHOD_BUFFERED,FILE_READ_ACCESS)
typedef struct {ULONG phys_lo,phys_hi,bytes,cached;} cx_read;
static PDEVICE_OBJECT device __attribute__((section(".data")))=0;
static NTSTATUS complete(PIRP irp,NTSTATUS st,ULONG_PTR info) {irp->IoStatus.Status=st;irp->IoStatus.Information=info;IoCompleteRequest(irp,IO_NO_INCREMENT);return st;}
static NTSTATUS __stdcall open_close(PDEVICE_OBJECT d,PIRP irp) {(void)d;return complete(irp,STATUS_SUCCESS,0);}
static NTSTATUS __stdcall control(PDEVICE_OBJECT d,PIRP irp) {
 PIO_STACK_LOCATION s=IoGetCurrentIrpStackLocation(irp);cx_read q;PHYSICAL_ADDRESS pa;volatile ULONG *src;ULONG *dst,k,n;
 (void)d;
 if(s->Parameters.DeviceIoControl.IoControlCode!=CX_IOCTL_READ||s->Parameters.DeviceIoControl.InputBufferLength<sizeof(q))return complete(irp,STATUS_INVALID_PARAMETER,0);
 q=*(cx_read*)irp->AssociatedIrp.SystemBuffer;
 if(!q.bytes||q.bytes>65536||(q.bytes&3)||(q.phys_lo&3)||s->Parameters.DeviceIoControl.OutputBufferLength<q.bytes)return complete(irp,STATUS_INVALID_PARAMETER,0);
 pa.LowPart=q.phys_lo;pa.HighPart=(LONG)q.phys_hi;
 src=(volatile ULONG*)MmMapIoSpace(pa,q.bytes,q.cached?MmCached:MmNonCached);
 if(!src)return complete(irp,STATUS_INSUFFICIENT_RESOURCES,0);
 dst=(ULONG*)irp->AssociatedIrp.SystemBuffer;n=q.bytes/4;
 for(k=0;k<n;k++)dst[k]=src[k];
 MmUnmapIoSpace((PVOID)src,q.bytes);
 return complete(irp,STATUS_SUCCESS,q.bytes);
}
static void __stdcall unload(PDRIVER_OBJECT drv) {
 UNICODE_STRING link;(void)drv;RtlInitUnicodeString(&link,L"\\DosDevices\\CXPHYS");IoDeleteSymbolicLink(&link);
 if(device)IoDeleteDevice(device);
}
NTSTATUS __stdcall DriverEntry(PDRIVER_OBJECT drv,PUNICODE_STRING path) {
 UNICODE_STRING name,link;NTSTATUS st;(void)path;
 RtlInitUnicodeString(&name,L"\\Device\\CXPHYS");RtlInitUnicodeString(&link,L"\\DosDevices\\CXPHYS");
 st=IoCreateDevice(drv,0,&name,FILE_DEVICE_UNKNOWN,0,FALSE,&device);if(!NT_SUCCESS(st))return st;
 st=IoCreateSymbolicLink(&link,&name);if(!NT_SUCCESS(st)){IoDeleteDevice(device);device=0;return st;}
 drv->MajorFunction[IRP_MJ_CREATE]=open_close;drv->MajorFunction[IRP_MJ_CLOSE]=open_close;drv->MajorFunction[IRP_MJ_DEVICE_CONTROL]=control;
 drv->DriverUnload=unload;
 return STATUS_SUCCESS;
}
