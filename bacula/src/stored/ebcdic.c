/*
 * Taken from the public domain ansitape program for 
 *   integration into Bacula. KES - Mar 2005
 */

/* Mapping of EBCDIC codes to ASCII equivalents. */
static char to_ascii_table[256] = {
   '\000', '\001', '\002', '\003',
   '\234', '\011', '\206', '\177',
   '\227', '\215', '\216', '\013',
   '\014', '\015', '\016', '\017',
   '\020', '\021', '\022', '\023',
   '\235', '\205', '\010', '\207',
   '\030', '\031', '\222', '\217',
   '\034', '\035', '\036', '\037',
   '\200', '\201', '\202', '\203',
   '\204', '\012', '\027', '\033',
   '\210', '\211', '\212', '\213',
   '\214', '\005', '\006', '\007',
   '\220', '\221', '\026', '\223',
   '\224', '\225', '\226', '\004',
   '\230', '\231', '\232', '\233',
   '\024', '\025', '\236', '\032',
   '\040', '\240', '\241', '\242',
   '\243', '\244', '\245', '\246',
   '\247', '\250', '\133', '\056',
   '\074', '\050', '\053', '\041',
   '\046', '\251', '\252', '\253',
   '\254', '\255', '\256', '\257',
   '\260', '\261', '\135', '\044',
   '\052', '\051', '\073', '\136',
   '\055', '\057', '\262', '\263',
   '\264', '\265', '\266', '\267',
   '\270', '\271', '\174', '\054',
   '\045', '\137', '\076', '\077',
   '\272', '\273', '\274', '\275',
   '\276', '\277', '\300', '\301',
   '\302', '\140', '\072', '\043',
   '\100', '\047', '\075', '\042',
   '\303', '\141', '\142', '\143',
   '\144', '\145', '\146', '\147',
   '\150', '\151', '\304', '\305',
   '\306', '\307', '\310', '\311',
   '\312', '\152', '\153', '\154',
   '\155', '\156', '\157', '\160',
   '\161', '\162', '\313', '\314',
   '\315', '\316', '\317', '\320',
   '\321', '\176', '\163', '\164',
   '\165', '\166', '\167', '\170',
   '\171', '\172', '\322', '\323',
   '\324', '\325', '\326', '\327',
   '\330', '\331', '\332', '\333',
   '\334', '\335', '\336', '\337',
   '\340', '\341', '\342', '\343',
   '\344', '\345', '\346', '\347',
   '\173', '\101', '\102', '\103',
   '\104', '\105', '\106', '\107',
   '\110', '\111', '\350', '\351',
   '\352', '\353', '\354', '\355',
   '\175', '\112', '\113', '\114',
   '\115', '\116', '\117', '\120',
   '\121', '\122', '\356', '\357',
   '\360', '\361', '\362', '\363',
   '\134', '\237', '\123', '\124',
   '\125', '\126', '\127', '\130',
   '\131', '\132', '\364', '\365',
   '\366', '\367', '\370', '\371',
   '\060', '\061', '\062', '\063',
   '\064', '\065', '\066', '\067',
   '\070', '\071', '\372', '\373',
   '\374', '\375', '\376', '\377'
};


/* Mapping of ASCII codes to EBCDIC equivalents. */
static char to_ebcdic_table[256] = {
   '\000', '\001', '\002', '\003',
   '\067', '\055', '\056', '\057',
   '\026', '\005', '\045', '\013',
   '\014', '\015', '\016', '\017',
   '\020', '\021', '\022', '\023',
   '\074', '\075', '\062', '\046',
   '\030', '\031', '\077', '\047',
   '\034', '\035', '\036', '\037',
   '\100', '\117', '\177', '\173',
   '\133', '\154', '\120', '\175',
   '\115', '\135', '\134', '\116',
   '\153', '\140', '\113', '\141',
   '\360', '\361', '\362', '\363',
   '\364', '\365', '\366', '\367',
   '\370', '\371', '\172', '\136',
   '\114', '\176', '\156', '\157',
   '\174', '\301', '\302', '\303',
   '\304', '\305', '\306', '\307',
   '\310', '\311', '\321', '\322',
   '\323', '\324', '\325', '\326',
   '\327', '\330', '\331', '\342',
   '\343', '\344', '\345', '\346',
   '\347', '\350', '\351', '\112',
   '\340', '\132', '\137', '\155',
   '\171', '\201', '\202', '\203',
   '\204', '\205', '\206', '\207',
   '\210', '\211', '\221', '\222',
   '\223', '\224', '\225', '\226',
   '\227', '\230', '\231', '\242',
   '\243', '\244', '\245', '\246',
   '\247', '\250', '\251', '\300',
   '\152', '\320', '\241', '\007',
   '\040', '\041', '\042', '\043',
   '\044', '\025', '\006', '\027',
   '\050', '\051', '\052', '\053',
   '\054', '\011', '\012', '\033',
   '\060', '\061', '\032', '\063',
   '\064', '\065', '\066', '\010',
   '\070', '\071', '\072', '\073',
   '\004', '\024', '\076', '\341',
   '\101', '\102', '\103', '\104',
   '\105', '\106', '\107', '\110',
   '\111', '\121', '\122', '\123',
   '\124', '\125', '\126', '\127',
   '\130', '\131', '\142', '\143',
   '\144', '\145', '\146', '\147',
   '\150', '\151', '\160', '\161',
   '\162', '\163', '\164', '\165',
   '\166', '\167', '\170', '\200',
   '\212', '\213', '\214', '\215',
   '\216', '\217', '\220', '\232',
   '\233', '\234', '\235', '\236',
   '\237', '\240', '\252', '\253',
   '\254', '\255', '\256', '\257',
   '\260', '\261', '\262', '\263',
   '\264', '\265', '\266', '\267',
   '\270', '\271', '\272', '\273',
   '\274', '\275', '\276', '\277',
   '\312', '\313', '\314', '\315',
   '\316', '\317', '\332', '\333',
   '\334', '\335', '\336', '\337',
   '\352', '\353', '\354', '\355',
   '\356', '\357', '\372', '\373',
   '\374', '\375', '\376', '\377'
};


/*
 * Convert from ASCII to EBCDIC 
 */
void ascii_to_ebcdic(char *dst, char *src, int count)
{
   while (count--) {
      *dst++ = to_ebcdic_table[0377 & *src++];
   }
}


/*
 * Convert from EBCDIC to ASCII
 */
void ebcdic_to_ascii(char *dst, char *src, int count)
{
   while (count--) {
      *dst++ = to_ascii_table[0377 & *src++];
   }
}
