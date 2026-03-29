
kernel.elf:     file format elf64-littleriscv


Disassembly of section .text:

0000000080200000 <_start>:
    80200000:	00002117          	auipc	sp,0x2
    80200004:	00010113          	mv	sp,sp
    80200008:	006000ef          	jal	8020000e <kmain>
    8020000c:	a001                	j	8020000c <_start+0xc>

000000008020000e <kmain>:
    8020000e:	1141                	addi	sp,sp,-16 # 80201ff0 <stack+0xff0>
    80200010:	00000517          	auipc	a0,0x0
    80200014:	1e050513          	addi	a0,a0,480 # 802001f0 <sys_exit+0x8>
    80200018:	e406                	sd	ra,8(sp)
    8020001a:	070000ef          	jal	8020008a <print_str>
    8020001e:	00000797          	auipc	a5,0x0
    80200022:	10e78793          	addi	a5,a5,270 # 8020012c <kernel_entry>
    80200026:	10579073          	csrw	stvec,a5
    8020002a:	00000517          	auipc	a0,0x0
    8020002e:	1de50513          	addi	a0,a0,478 # 80200208 <sys_exit+0x20>
    80200032:	058000ef          	jal	8020008a <print_str>
    80200036:	00000797          	auipc	a5,0x0
    8020003a:	15e78793          	addi	a5,a5,350 # 80200194 <user_entry>
    8020003e:	14179073          	csrw	sepc,a5
    80200042:	100027f3          	csrr	a5,sstatus
    80200046:	eff7f793          	andi	a5,a5,-257
    8020004a:	10079073          	csrw	sstatus,a5
    8020004e:	00000517          	auipc	a0,0x0
    80200052:	1ca50513          	addi	a0,a0,458 # 80200218 <sys_exit+0x30>
    80200056:	034000ef          	jal	8020008a <print_str>
    8020005a:	10200073          	sret
    8020005e:	00000517          	auipc	a0,0x0
    80200062:	1da50513          	addi	a0,a0,474 # 80200238 <sys_exit+0x50>
    80200066:	024000ef          	jal	8020008a <print_str>
    8020006a:	a001                	j	8020006a <kmain+0x5c>

000000008020006c <sbi_call>:
    8020006c:	88aa                	mv	a7,a0
    8020006e:	882e                	mv	a6,a1
    80200070:	8532                	mv	a0,a2
    80200072:	85b6                	mv	a1,a3
    80200074:	863a                	mv	a2,a4
    80200076:	00000073          	ecall
    8020007a:	8082                	ret

000000008020007c <putchar>:
    8020007c:	4581                	li	a1,0
    8020007e:	4601                	li	a2,0
    80200080:	4801                	li	a6,0
    80200082:	4885                	li	a7,1
    80200084:	00000073          	ecall
    80200088:	8082                	ret

000000008020008a <print_str>:
    8020008a:	87aa                	mv	a5,a0
    8020008c:	00054503          	lbu	a0,0(a0)
    80200090:	c919                	beqz	a0,802000a6 <print_str+0x1c>
    80200092:	0785                	addi	a5,a5,1
    80200094:	4581                	li	a1,0
    80200096:	4601                	li	a2,0
    80200098:	4801                	li	a6,0
    8020009a:	4885                	li	a7,1
    8020009c:	00000073          	ecall
    802000a0:	0007c503          	lbu	a0,0(a5)
    802000a4:	f57d                	bnez	a0,80200092 <print_str+0x8>
    802000a6:	8082                	ret

00000000802000a8 <trap_handler>:
    802000a8:	14202773          	csrr	a4,scause
    802000ac:	37ab77b7          	lui	a5,0x37ab7
    802000b0:	078a                	slli	a5,a5,0x2
    802000b2:	eef78793          	addi	a5,a5,-273 # 37ab6eef <_start-0x48749111>
    802000b6:	e91c                	sd	a5,16(a0)
    802000b8:	47a1                	li	a5,8
    802000ba:	00f70363          	beq	a4,a5,802000c0 <trap_handler+0x18>
    802000be:	8082                	ret
    802000c0:	755c                	ld	a5,168(a0)
    802000c2:	4715                	li	a4,5
    802000c4:	04f76d63          	bltu	a4,a5,8020011e <trap_handler+0x76>
    802000c8:	00000717          	auipc	a4,0x0
    802000cc:	18870713          	addi	a4,a4,392 # 80200250 <sys_exit+0x68>
    802000d0:	078a                	slli	a5,a5,0x2
    802000d2:	97ba                	add	a5,a5,a4
    802000d4:	439c                	lw	a5,0(a5)
    802000d6:	1141                	addi	sp,sp,-16
    802000d8:	e406                	sd	ra,8(sp)
    802000da:	97ba                	add	a5,a5,a4
    802000dc:	8782                	jr	a5
    802000de:	05a00793          	li	a5,90
    802000e2:	f93c                	sd	a5,112(a0)
    802000e4:	141027f3          	csrr	a5,sepc
    802000e8:	0791                	addi	a5,a5,4
    802000ea:	14179073          	csrw	sepc,a5
    802000ee:	60a2                	ld	ra,8(sp)
    802000f0:	0141                	addi	sp,sp,16
    802000f2:	8082                	ret
    802000f4:	00000517          	auipc	a0,0x0
    802000f8:	15450513          	addi	a0,a0,340 # 80200248 <sys_exit+0x60>
    802000fc:	f8fff0ef          	jal	8020008a <print_str>
    80200100:	a001                	j	80200100 <trap_handler+0x58>
    80200102:	793c                	ld	a5,112(a0)
    80200104:	7d38                	ld	a4,120(a0)
    80200106:	97ba                	add	a5,a5,a4
    80200108:	f93c                	sd	a5,112(a0)
    8020010a:	bfe9                	j	802000e4 <trap_handler+0x3c>
    8020010c:	7928                	ld	a0,112(a0)
    8020010e:	f7dff0ef          	jal	8020008a <print_str>
    80200112:	bfc9                	j	802000e4 <trap_handler+0x3c>
    80200114:	07054503          	lbu	a0,112(a0)
    80200118:	f65ff0ef          	jal	8020007c <putchar>
    8020011c:	b7e1                	j	802000e4 <trap_handler+0x3c>
    8020011e:	141027f3          	csrr	a5,sepc
    80200122:	0791                	addi	a5,a5,4
    80200124:	14179073          	csrw	sepc,a5
    80200128:	8082                	ret
	...

000000008020012c <kernel_entry>:
    8020012c:	7171                	addi	sp,sp,-176
    8020012e:	e006                	sd	ra,0(sp)
    80200130:	e40a                	sd	sp,8(sp)
    80200132:	e822                	sd	s0,16(sp)
    80200134:	ec26                	sd	s1,24(sp)
    80200136:	f04a                	sd	s2,32(sp)
    80200138:	f44e                	sd	s3,40(sp)
    8020013a:	f852                	sd	s4,48(sp)
    8020013c:	fc56                	sd	s5,56(sp)
    8020013e:	e0da                	sd	s6,64(sp)
    80200140:	e4de                	sd	s7,72(sp)
    80200142:	e8e2                	sd	s8,80(sp)
    80200144:	ece6                	sd	s9,88(sp)
    80200146:	f0ea                	sd	s10,96(sp)
    80200148:	f4ee                	sd	s11,104(sp)
    8020014a:	f8aa                	sd	a0,112(sp)
    8020014c:	fcae                	sd	a1,120(sp)
    8020014e:	e132                	sd	a2,128(sp)
    80200150:	e536                	sd	a3,136(sp)
    80200152:	e93a                	sd	a4,144(sp)
    80200154:	ed3e                	sd	a5,152(sp)
    80200156:	f142                	sd	a6,160(sp)
    80200158:	f546                	sd	a7,168(sp)
    8020015a:	850a                	mv	a0,sp
    8020015c:	f4dff0ef          	jal	802000a8 <trap_handler>
    80200160:	6082                	ld	ra,0(sp)
    80200162:	6122                	ld	sp,8(sp)
    80200164:	6442                	ld	s0,16(sp)
    80200166:	64e2                	ld	s1,24(sp)
    80200168:	7902                	ld	s2,32(sp)
    8020016a:	79a2                	ld	s3,40(sp)
    8020016c:	7a42                	ld	s4,48(sp)
    8020016e:	7ae2                	ld	s5,56(sp)
    80200170:	6b06                	ld	s6,64(sp)
    80200172:	6ba6                	ld	s7,72(sp)
    80200174:	6c46                	ld	s8,80(sp)
    80200176:	6ce6                	ld	s9,88(sp)
    80200178:	7d06                	ld	s10,96(sp)
    8020017a:	7da6                	ld	s11,104(sp)
    8020017c:	7546                	ld	a0,112(sp)
    8020017e:	75e6                	ld	a1,120(sp)
    80200180:	660a                	ld	a2,128(sp)
    80200182:	66aa                	ld	a3,136(sp)
    80200184:	674a                	ld	a4,144(sp)
    80200186:	67ea                	ld	a5,152(sp)
    80200188:	780a                	ld	a6,160(sp)
    8020018a:	78aa                	ld	a7,168(sp)
    8020018c:	614d                	addi	sp,sp,176
    8020018e:	10200073          	sret
    80200192:	0001                	nop

0000000080200194 <user_entry>:
    80200194:	016000ef          	jal	802001aa <user_main>
    80200198:	a001                	j	80200198 <user_entry+0x4>

000000008020019a <test_func>:
    8020019a:	4505                	li	a0,1
    8020019c:	4589                	li	a1,2
    8020019e:	4891                	li	a7,4
    802001a0:	00000073          	ecall
    802001a4:	55800513          	li	a0,1368
    802001a8:	8082                	ret

00000000802001aa <user_main>:
    802001aa:	4505                	li	a0,1
    802001ac:	4589                	li	a1,2
    802001ae:	4891                	li	a7,4
    802001b0:	00000073          	ecall
    802001b4:	03d00513          	li	a0,61
    802001b8:	4885                	li	a7,1
    802001ba:	00000073          	ecall
    802001be:	4501                	li	a0,0
    802001c0:	4895                	li	a7,5
    802001c2:	00000073          	ecall
    802001c6:	8082                	ret

00000000802001c8 <sys_putchar>:
    802001c8:	4885                	li	a7,1
    802001ca:	00000073          	ecall
    802001ce:	8082                	ret

00000000802001d0 <sys_printstr>:
    802001d0:	4889                	li	a7,2
    802001d2:	00000073          	ecall
    802001d6:	8082                	ret

00000000802001d8 <sys_get_magic>:
    802001d8:	488d                	li	a7,3
    802001da:	00000073          	ecall
    802001de:	8082                	ret

00000000802001e0 <sys_add>:
    802001e0:	4891                	li	a7,4
    802001e2:	00000073          	ecall
    802001e6:	8082                	ret

00000000802001e8 <sys_exit>:
    802001e8:	4895                	li	a7,5
    802001ea:	00000073          	ecall
    802001ee:	8082                	ret
