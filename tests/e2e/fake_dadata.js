// Подставной DaData для сценариев: node fake_dadata.js <порт>.
// Ключ «good»; ИНН 7707083893 - организация, 500100732259 - ИП; БИК 044525225 - банк. Остальное не найдено.
const http = require('http');
const port = Number(process.argv[2]);
const LEGAL = {
    type: 'LEGAL', inn: '7707083893', kpp: '773601001', ogrn: '1027700132195',
    opf: {full: 'Общество с ограниченной ответственностью', short: 'ООО'},
    name: {full: 'ТОРГОВЫЙ ДОМ "ВОСХОД"', short: 'ТД "ВОСХОД"'},
    management: {name: 'ПЕТРОВ ПЁТР ПАВЛОВИЧ', post: 'ГЕНЕРАЛЬНЫЙ ДИРЕКТОР'},
    address: {value: 'г Москва, ул Ленина, д 1', unrestricted_value: '117312, г Москва, ул Ленина, д 1'},
};
const INDIVIDUAL = {
    type: 'INDIVIDUAL', inn: '500100732259', ogrn: '304500116000157',
    fio: {surname: 'Сидорова', name: 'Анна', patronymic: 'Сергеевна'},
    address: {value: 'Московская обл, г Химки'},
};
http.createServer((req, res) => {
    let body = '';
    req.on('data', d => body += d);
    req.on('end', () => {
        if (req.headers.authorization !== 'Token good') {
            res.writeHead(403);
            res.end();
            return;
        }
        const query = JSON.parse(body).query;
        const suggestions = [];
        if (req.url.endsWith('/findById/party') && query === LEGAL.inn) suggestions.push({value: 'ТД', data: LEGAL});
        if (req.url.endsWith('/findById/party') && query === INDIVIDUAL.inn) suggestions.push({value: 'ИП', data: INDIVIDUAL});
        if (req.url.endsWith('/findById/bank') && query === '044525225') suggestions.push({value: 'Банк', data: {name: {payment: 'ПАО "Сбербанк"'}}});
        res.writeHead(200, {'Content-Type': 'application/json'});
        res.end(JSON.stringify({suggestions}));
    });
}).listen(port, '127.0.0.1');
