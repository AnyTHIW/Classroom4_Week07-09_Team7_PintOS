#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
typedef int32_t off_t;
#include "filesys/file.h"
#include "filesys/filesys.h"

void syscall_entry(void);
void syscall_handler(struct intr_frame *);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void syscall_init(void)
{
    write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48 |
                            ((uint64_t)SEL_KCSEG) << 32);
    write_msr(MSR_LSTAR, (uint64_t)syscall_entry);

    /* The interrupt service rountine should not serve any interrupts
     * until the syscall_entry swaps the userland stack to the kernel
     * mode stack. Therefore, we masked the FLAG_FL. */
    write_msr(MSR_SYSCALL_MASK,
              FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* The main system call interface */
void syscall_handler(struct intr_frame *f)
{
    // TODO: Your implementation goes here.
    // printf("system call!\n");

    uint64_t syscall_case = f->R.rax;

    switch (syscall_case)
    {
    case SYS_HALT:
        halt();
        break;
    case SYS_EXIT:
        exit(f->R.rdi);
        break;
    case SYS_FORK:
        // f->R.rax = fork(f->R.rbx);
        break;
    case SYS_EXEC:
        f->R.rax = exec(f->R.rdi);
        break;
    case SYS_WAIT:
        f->R.rax = wait(f->R.rdi);
        break;
    case SYS_CREATE:
        f->R.rax = create(f->R.rdi, f->R.rsi);
        break;
    case SYS_REMOVE:
        f->R.rax = remove(f->R.rdi);
        break;
    case SYS_OPEN:
        f->R.rax = open(f->R.rdi);
        break;
    case SYS_FILESIZE:
        f->R.rax = filesize(f->R.rdi);
        break;
    case SYS_READ:
        f->R.rax = read(f->R.rdi, f->R.rsi, f->R.rdx);
        break;
    case SYS_WRITE:
        f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);
        break;
    case SYS_SEEK:
        seek((int)f->R.rdi, (unsigned)f->R.rsi);
        break;
    case SYS_TELL:
        f->R.rax = tell((int)f->R.rdi);
        break;
    case SYS_CLOSE:
        close(f->R.rdi);
        break;
    default:
        break;
    }

    // thread_exit();
}

void halt()
{
    power_off();
}

void exit(int status)
{
    struct thread *curr = thread_current();

    printf("%s: exit(%d)\n", curr->name, status);

    thread_exit();
}

// pid_t fork(const char *thread_name)
// {
//     char **child = thread_name;

//     curr->의 자식 thread를 찾는다.
//     if 자식 프로세스를 찾고 성공한 반환값 == 0 자식 프로세스의 복제시작.
//     자식 프로세스의 복제 끝 return child_pid = 자식 thread의 pid.else return TID_ERROR
// }

int exec(const char *cmd_line)
{
    // cmd_line의 프로세스를 실행(지금 꺼 말고).스레드 이름 변경 X.FD는 열린상태.strtok_r(cmd_line);
    bool success = process_exec(cmd_line);
    // 인수 포함.성공시 노 반환.실패 시 - 1 반환.
    if (success == true)
        return;
    else
        return -1;
}

// 구현 어려움
int wait(pid_t pid)
{
    // pid의 종료를 기다린다.
    while (pid == 0)
    {
    }

    // if (pid가 exit() 호출하지 않음 && 커널에 의해 죽음)
    //     return -1;

    // // 종료된 프로세스로 전달된 pid를 반환한다.
    // return this_status = process_exit(pid);
}

bool create(const char *file, unsigned initial_size)
{
    bool success = filesys_create(file, (int32_t)initial_size);

    if (success == true)
        return true;
    else
        return false;
}

bool remove(const char *file)
{
    bool success = filesys_remove(file);

    if (success == true)
    {
        file_close(file);
        return true;
    }
    else
        return false;
}

int open(const char *file)
{
    // 파일 디스크립터 테이블 PTE 로 쓰고 ( thread구조체 마다) 불러오기
    struct thread *curr = thread_current();
    struct file *f_ptr = filesys_open(file);

    int success_cnt = 2;
    if (f_ptr != NULL)
    {
        success_cnt++;
        curr->f_d_t[success_cnt] = f_ptr;
        return success_cnt;
    }
    else
        return -1;
}

int filesize(int fd)
{
    struct thread *curr = thread_current();
    return file_length(curr->f_d_t[fd]) * sizeof(char *);
}

int read(int fd, void *buffer, unsigned size)
{
    struct thread *curr = thread_current();

    int read_bytes = -1;

    if (fd == 0)
        read_bytes = input_getc();
    else
        read_bytes = file_read(curr->f_d_t[fd], buffer, size);

    if (read_bytes == -1)
        return -1;

    return read_bytes;
}

int write(int fd, const void *buffer, unsigned size)
{
    struct thread *curr = thread_current();
    // stdinput input_getc(); 0, 일때
    int written_bytes = -1;

    if (fd == 1)
        written_bytes = input_putc();
    else
        written_bytes = file_write(curr->f_d_t[fd], buffer, size);

    if (written_bytes == -1)
        return -1;

    return written_bytes;
}

void seek(int fd, unsigned position)
{
    unsigned old_position = position;
    struct thread *curr = thread_current();
    char buffer[128];

    file_seek(curr->f_d_t[fd], (off_t)position);

    int this_zero_byte = file_read_at(curr->f_d_t[fd], buffer, file_length(curr->f_d_t[fd]) - position, position);

    if (file_length(curr->f_d_t[fd]) < (old_position + position))
        exit(-1);

    return fd;
}

unsigned tell(int fd)
{
    //  열려진 파일 fd에서 읽히거나 써질 다음 바이트의 위치를 반환합니다.
    struct thread *curr = thread_current();

    off_t next_position = file_tell(curr->f_d_t[fd]);

    return (unsigned)next_position;
}

void close(int fd)
{
    struct thread *curr = thread_current();
    // file_close 쓰기
    file_close(curr->f_d_t[fd]);
}
