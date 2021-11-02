//---------------------------------- 
//номера операционных регистров 
//---------------------------------- 
#define NUMBER_CHECK_DAY 0 //количество чеков (продажа, покупка ... ) 4 регистра 
#define NUMBER_CHECK (NUMBER_CHECK_DAY+4) //номер чека 4 регистра 
#define NUMBER_DOC (NUMBER_CHECK+4) //сквозной номер документа 
#define NUMBER_CASHIN_DAY (NUMBER_DOC+1) //количество внесений за смену 
#define NUMBER_CASHOUT_DAY (NUMBER_CASHIN_DAY+1) //количество выплат за смену 
#define NUMBER_CASHIN (NUMBER_CASHOUT_DAY+1) //номер внесения 
#define NUMBER_CASHOUT (NUMBER_CASHIN+1) //номер выплаты 
#define NUMBER_ANNUL (NUMBER_CASHOUT+1) //количество аннулированных чеков 
#define NUMBER_REP (NUMBER_ANNUL+1) //номер сменного отчета без гашения 
#define NUMBER_CLOSE (NUMBER_REP+1) //номер сменного отчета до фискализации 
#define NUMBER_RESET (NUMBER_CLOSE+1) //номер общего гашения 
#define NUMBER_FULL_REP (NUMBER_RESET+1) //номер полного фискального отчета 
#define NUMBER_SMALL_REP (NUMBER_FULL_REP+1) //номер сокращенного фискально отчета 
#define NUMBER_TEST (NUMBER_SMALL_REP+1) //номер тестового прогона 
#define NUMBER_REGISTR (NUMBER_TEST+1) //номер снятия показаний операционных регистров 
#define NUMBER_DEP (NUMBER_REGISTR+1) //номер отчета по секциям 
#define NUMBER_OPER (NUMBER_DEP+1) //номер отчета по кассирам 
#define NUMBER_ART (NUMBER_OPER+1) //номер отчета по товарам 
#define NUMBER_HOUR (NUMBER_ART+1) //номер почасового отчета 
#define NUMBER_STORNO (NUMBER_HOUR+1) //количество операций сторно 4 регистра 
#define OP_REG_END (NUMBER_STORNO+4) //общее количество регистров 
//операционыые регистры присутствуют во всех вариантах 
//количество операционных регистров 
#define OpRegAmount (OP_REG_END+1) 


//---------------------------------- 
//номера денежных регистров 
//---------------------------------- 
#define CHECK_OPL_TYPE 0 //регистры накопления по типам оплаты 
#define CHECK_TAX_GRP (CHECK_OPL_TYPE+4) //регистры накопления налогов 

#define SMENA_TAX_GRP (CHECK_TAX_GRP+4) //налоговые накопления за смену по 4 типам чеков (16 регистров 4 налоговых группы Х 4 типа чека) 
#define SMENA_CASH (SMENA_TAX_GRP+16) //наличность в кассе 

#define CASH_IN (SMENA_CASH+1) //внесения 
#define CASH_OUT (CASH_IN+1) //выплаты 

#define GLOBAL_SUMMA (CASH_OUT+1) //необнуляемый итог до фискализации 

#define ANNUL_SUMMA (GLOBAL_SUMMA+1) //сумма аннулированных чеков

//----------------------------------
//флаги
//----------------------------------
#define FLAG_RECEIPT	0x0001
#define FLAG_SESSION	0x0040
