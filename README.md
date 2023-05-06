# 파일시스템의 Open, Read, Write, Make, Remove 기능 구현

# 작업 기간
- 1차 과제: 2022년 5월 11일 ~ 5월 26일
- 2차 과제: 2022년 6월 1일 ~ 6월 23일

# 주요 업무
- 1차 과제: 가상 디스크에서 블록 단위로 데이터(I-node, Directory entry, block)를 읽을 수도 있고 저장할 수 있는 파일시스템 메타데이터를 관리하는 API구현

- 2차 과제: 과제 1에서 구현한 API를 사용해 Open, Read, Write, Make, Remove와 같은 실제 파일시스템에서 제공하는 시스템 콜 구현

# 사용언어 및 개발환경
- C언어를 기반으로 Linux(Ubuntu) 환경에서 개발

# 발생 문제 및 해결 과정
- 2차 과제에서 디렉토리에 생성되는 파일의 개수가 32개를 넘어가는 경우, 그 이상으로 파일이 생성되지 않는 문제가 발생해 디버깅을 진행하려고 했지만 리눅스 환경에서의 디버깅이 처음이었음

- 과제 마감기한이 얼마 남지 않아 익숙한 IDE인 visual studio로 디버깅을 진행한다고 가정하고, 그 진행 사항과 데이터의 변화를 수기로 기록하며 디버깅

- 문제의 원인은 i-node구조체에서 사용되었던 간접 블록 포인터(indirectBlockPtr)를 사용하지 않고 구현을 진행했다는 것을 발견
```c
typedef struct _Inode {
    int          allocBlocks;
    int          size;
    FileType     type;
    int          dirBlockPtr[NUM_OF_DIRECT_BLOCK_PTR];   // block pointers
    int          indirectBlockPtr;               // indirect block pointer
} Inode;

64byte
```
- 생성할 파일이 많아지는 경우에 간접 블록 포인터를 할당하고 i-node를 저장하는 코드를 추가

# 테스트 케이스 실행 예시
테스트 케이스는 파일 시스템을 구현한 Open, Read, Write, Make, Remove기능들이 잘 동작하는지 확인하는 역할을 하고, 해당 과제의 채점기준이기도 했습니다.

테스트케이스 실행은 Linux환경에서 진행해주셔야 합니다.

```
git clone https://github.com/JungHun98/FileSystem-HW

// 실행파일 hw2 생성
make 

// testcase 실행
./hw2 createfs 1
./hw2 openfs 2
./hw2 openfs 3
./hw2 openfs 4
```

# 실행 영상

![제목 없는 동영상 - Clipchamp로 제작](https://user-images.githubusercontent.com/97653343/230119681-59008741-0c91-4c86-9529-13fa455f92fd.gif)
