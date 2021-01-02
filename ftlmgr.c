#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "flash.h"
#include "fdevicedriver.c"

FILE *flashfp;	// fdevicedriver.c에서 사용
int check_option(int argc, char *argv[]); //옵션 확인 함수

int cOption = 0;
int rOption = 0;
int wOption = 0;
int eOption = 0;

int main(int argc, char *argv[])
{	
	char sectorbuf[SECTOR_SIZE];
	char pagebuf[PAGE_SIZE];
	char sparebuf[SPARE_SIZE];
	char *blockbuf;
	int fsize;

	blockbuf = (char *)malloc(sizeof(char)*BLOCK_SIZE);

	if(check_option(argc,argv)==0) //해당 옵션이 없거나 필요한 인자를 받지 못한 경우 종료
		exit(1);

	if(cOption){ //파일 오픈
		int block_count = 1;
		
		if((flashfp = fopen(argv[2], "w+")) == NULL){ //파일을 읽기 쓰기 가능하도록 오픈 기존 파일 있으면 새로 생성
                	 fprintf(stderr, "fopen error for %s\n", argv[2]);
                	 exit(1);}

        	 memset((void *)blockbuf, (char)0xFF, BLOCK_SIZE); //block버퍼에 블록크기만큼 0xff초기화

        	 while(block_count<=atoi(argv[3])){

                	fwrite((void *)blockbuf, BLOCK_SIZE, 1, flashfp); //파일에 block버퍼 쓰기

                	fseek(flashfp, BLOCK_SIZE * block_count , SEEK_SET); //파일 포인터 위치 이동

                 	block_count++;//블록 갯수 증가
		 }

	}
		
	if(wOption){ //페이지에 데이터 쓰기
		int free_block = -1; //비어있는 블록
		int write_block; //데이터를 쓰려는 페이지의 블록
		int pbn_in_block; //데이터를 쓰려는 페이지의 블록 안에서의 해당 페이지의 번호
		int data=0; //입력받은 페이지에 있는 데이터 확인
		int other_data; //블록의 데이터 확인

		if((flashfp = fopen(argv[2], "r+")) == NULL){ //파일을 읽기와 쓰기가 가능하도록 오픈
			fprintf(stderr, "fopen error for %s\n", argv[2]);
			exit(1);}
        
		fseek(flashfp, 0, SEEK_END);
		fsize = ftell(flashfp); //파일 크기 구하기

		if(((atoi(argv[3])+1)*PAGE_SIZE)>fsize){ //입력받은 페이지를 안가지고 있으면 에러처리
			fprintf(stderr, "%s dont't have %s page\n", argv[2], argv[3]);
			exit(1);
		}

		if((strlen(argv[4])>=SECTOR_SIZE) || (strlen(argv[5])>=SPARE_SIZE)){ //버퍼의 크기보다 입력받은 데이터의 크기가 크다면 에러
			fprintf(stderr, "data size(sector,spare) > buffer size(sector,spare)\n");
			exit(1);
		}

		write_block = (atoi(argv[3])/4); //데이터를 쓰려는 페이지의 블록 구하기
		pbn_in_block = (atoi(argv[3])%4); //데이터를 쓰려는 페이지의 블록 안에서의 해당 페이지의 번호 구하기

		dd_read(atoi(argv[3]), pagebuf); //쓰려는 페이지 가져오기
		
		for(int i=0; i<PAGE_SIZE; i++){ //쓰려는 페이지가 비어있는지 검사
			if(pagebuf[data]!=(char)0xFF){
				data++;
				break;}
		}

		if(data!=0){ //쓰려는 페이지가 비어있지 않다면
			for(int i=0; i<(fsize/BLOCK_SIZE); i++){ //비어있는 블록 찾기
				other_data = 0;

				memset((void *)blockbuf, (char)0xFF, BLOCK_SIZE); //비어있는 블록버퍼에
				fseek(flashfp, BLOCK_SIZE*i, SEEK_SET); //각각의 블록의 위치로 이동하여
				fread((void *)blockbuf, BLOCK_SIZE, 1, flashfp); //블록의 데이터를 블록 버퍼에 복사

				for(int i=0; i<BLOCK_SIZE; i++){ //블록 안에 데이터가 있다면 해당 블록을 사용할 수 없으므로 반복문 나가기
					if(blockbuf[i]!=(char)0xFF){
						other_data++;
						break;}
				}

				if(other_data==0){ //블록에 데이터가 없다면 free_block으로 사용하도록 한다.
					free_block = i;
					break;}
			}

			if(free_block == -1){ //free_block이 없다면 데이터를 쓸 수 없으므로 종료
				printf("%s don't have free block\n", argv[2]);
				exit(1);}
		

			for(int i=0; i<4; i++){ //인자로 받은 페이지 이외의 페이지의 데이터를 읽어서 비어있는 블록의 페이지에 복사
				if(i==pbn_in_block) //인자로 받은 페이지인 경우는 복사 안함
					continue;

				memset((void *)pagebuf, (char)0xFF, PAGE_SIZE);
				fseek(flashfp, (BLOCK_SIZE*write_block)+(PAGE_SIZE*i), SEEK_SET);
				fread((void *)pagebuf, PAGE_SIZE, 1, flashfp); //페이지의 데이터 읽기

				fseek(flashfp, (BLOCK_SIZE*free_block)+(PAGE_SIZE*i), SEEK_SET);
				fwrite((void *)pagebuf, PAGE_SIZE, 1, flashfp); //비어있는 블록의 페이지에 데이터 쓰기
			}

			memset((void *)blockbuf, (char)0xFF, BLOCK_SIZE);
			fseek(flashfp, BLOCK_SIZE*write_block, SEEK_SET);
			fwrite((void *)blockbuf, BLOCK_SIZE, 1, flashfp); //입력받은 페이지가 있는 블록 초기화

			memset((void *)pagebuf, (char)0xFF, PAGE_SIZE); //페이지 버퍼 초기화

			strncpy(sectorbuf, argv[4], strlen(argv[4])); //sector버퍼에 입력받은 데이터 넣기

			for(int i = strlen(argv[4]); i<SECTOR_SIZE-1; i++) //sector버퍼에 남은 공간 채우기
				sectorbuf[i] = (char)0xFF;

			strncpy(sparebuf, argv[5], strlen(argv[5])); //spare버퍼에 입력받은 데이터 넣기

			for(int i = strlen(argv[5]); i<SPARE_SIZE-1; i++) //spare버퍼에 남은 공간 채우기
				sparebuf[i] = (char)0xFF;

			sprintf(pagebuf,"%s%s",sectorbuf, sparebuf); //페이저 버퍼에 sector,spare 데이터 넣기

			if(dd_write(atoi(argv[3]), pagebuf)==-1){ //파일의 해당 페이지에 데이터 쓰기 안되면 에러
				fprintf(stderr, "page write error\n");
				exit(1);
			}

			for(int i=0; i<4; i++){ //다른 블록에 복사했던 데이터들을 기존의 블록에 다시 복사
				if(i==pbn_in_block) //쓰려는 페이지는 복사 안함
					continue;

				memset((void *)pagebuf, (char)0xFF, PAGE_SIZE);
				fseek(flashfp, (BLOCK_SIZE*free_block)+(PAGE_SIZE*i), SEEK_SET);
				fread((void *)pagebuf, PAGE_SIZE, 1, flashfp); //다른 블록에 복사했던 페이지의 데이터 읽기

				fseek(flashfp, (BLOCK_SIZE*write_block)+(PAGE_SIZE*i), SEEK_SET);
				fwrite((void *)pagebuf, PAGE_SIZE, 1, flashfp); //기존 블록의 페이지에 다시 데이터 쓰기
			}

			memset((void *)blockbuf, (char)0xFF, BLOCK_SIZE);
			fseek(flashfp, (BLOCK_SIZE*free_block), SEEK_SET);
			fwrite((void *)blockbuf, BLOCK_SIZE, 1, flashfp); //복사했던 블록 초기화
		}

		else{ //비어있지 않다면
			memset((void *)pagebuf, (char)0xFF, PAGE_SIZE); //페이지 버퍼 초기화

			strncpy(sectorbuf, argv[4], strlen(argv[4])); //sector버퍼에 입력받은 데이터 넣기

			for(int i = strlen(argv[4]); i<SECTOR_SIZE-1; i++) //sector버퍼에 남은 공간 채우기
				sectorbuf[i] = (char)0xFF;

			strncpy(sparebuf, argv[5], strlen(argv[5])); //spare버퍼에 입력받은 데이터 넣기

			for(int i = strlen(argv[5]); i<SPARE_SIZE-1; i++) //spare버퍼에 남은 공간 채우기
				sparebuf[i] = (char)0xFF;

			sprintf(pagebuf,"%s%s",sectorbuf, sparebuf); //페이저 버퍼에 sector,spare 데이터 넣기

			if(dd_write(atoi(argv[3]), pagebuf)==-1){ //파일의 해당 페이지에 데이터 쓰기 안되면 에러
				fprintf(stderr, "page write error\n");
				exit(1);
			}
		}
	}
			

	if(rOption){ //페이지를 sector spare나눠서 읽기

		int a = 0;
		int b = 0;

   		if((flashfp = fopen(argv[2], "r+")) == NULL){ //파일을 읽기와 쓰기가 가능하도록 오픈
                	fprintf(stderr, "fopen error for %s\n", argv[2]);
              		exit(1);}

      		fseek(flashfp, 0, SEEK_END);
        	fsize = ftell(flashfp); //파일 크기 구하기

        	if(((atoi(argv[3])+1)*PAGE_SIZE)>fsize){ //입력받은 페이지를 안가지고 있으면 에러처리
                	fprintf(stderr, "%s dont't have %s page\n", argv[2], argv[3]);
                	exit(1);
		}

		if(dd_read(atoi(argv[3]), pagebuf)==-1){ //파일의 해당 페이지의 데이터 읽기 안되면 에러
			fprintf(stderr, "page read error\n");
			exit(1);
		}

		for(int i=0; i<SECTOR_SIZE-1; i++){ //sector의 데이터 출력
			if(pagebuf[i]!=(char)0xFF){
				printf("%c", pagebuf[i]);
				a++;}
			else
				break;
		}

		if(a!=0)
			printf(" ");

		for(int i=0; i<SPARE_SIZE-1; i++) //spare의 데이터 출력
		{
			if(pagebuf[i+511]!=(char)0xFF){
				printf("%c", pagebuf[i+511]);
				b++;}
			else
				break;
		}

		if(b!=0||a!=0)
			printf("\n");

	}
	
	if(eOption){ //블록 소거

		if((flashfp = fopen(argv[2], "r+")) == NULL){ //파일을 읽기와 쓰기가 가능하도록 오픈
			fprintf(stderr, "fopen error for %s\n", argv[2]);
			exit(1);}

		fseek(flashfp, 0, SEEK_END); 
		fsize = ftell(flashfp); //파일 크기 구하기

		if((fsize<((atoi(argv[3])+1)*4*PAGE_SIZE))){ //입력받은 블록을 안가지고 있으면 에러처리
			fprintf(stderr,"%s don't have %s block\n", argv[2], argv[3]);
			exit(1);
		}

		if(dd_erase(atoi(argv[3]))==-1){ //해당 블록 소거
			fprintf(stderr, "block erase error\n");
			exit(1);
		}
	}

	free(blockbuf); //동적메모리 해제
	return 0;
}

int check_option(int argc, char *argv[]){ //옵션 확인하는 함수
	char *c;
	c = argv[1];
	switch(*c){
			case 'c': //파일 만들기
				cOption = 1;
				if(argc != 4){
					fprintf(stderr, "usage: %s c <flashfile> <#blocks>\n", argv[0]);
					exit(1);}
				break;

			case 'w': //파일의 페이지에 데이터 쓰기
				wOption = 1;
				if(argc != 6){
					fprintf(stderr, "usage: %s w <flasefile> <ppn> <sectordata> <sparedata>\n",argv[0]);
					exit(1);}
				break;

			case 'r' : //파일의 페이지의 데이터 읽기
				rOption = 1;
				if(argc != 4){
					fprintf(stderr, "usage: %s r <flashfile> <ppn>\n", argv[0]);
					exit(1);}
				break;

			case 'e' : //파일의 페이지 삭제
				eOption = 1;
				if(argc != 4){
					fprintf(stderr, "usage: %s e <flasefile> <pbn>\n",argv[0]);
					exit(1);
				}
				break;

			default:
				printf("Unkown option\n");
				return 0;
		}
	return 1;
}
