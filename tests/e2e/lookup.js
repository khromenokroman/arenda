// Заполнение по ИНН и БИК: арендодатель из DaData (подсветка, родительный падеж), банк по БИК,
// сохранение; арендатор-ИП по ИНН с полом; у физлица кнопки по ИНН нет; ошибка «не найдено».
const input = name => document.querySelector(`form.card [name=${name}]`);
const lookup = what => document.querySelector(`button[data-lookup=${what}]`);
const message = () => document.getElementById('message');

input('inn').value = '7728168971';
lookup('party').click();
await sleep(600);
expect(message().className === 'error' && message().textContent.includes('ничего не найдено'), 'нет ошибки «не найдено»: ' + message().textContent);

input('inn').value = ' 7707083893 ';
lookup('party').click();
await sleep(600);
log.push('сообщение: ' + message().textContent);
expect(message().className === 'ok', 'заполнение не удалось: ' + message().textContent);
expect(input('name_short').value === 'ООО «ТД «ВОСХОД»»', 'краткое наименование: ' + input('name_short').value);
expect(input('signer_name_gen').value === 'Петрова Петра Павловича', 'ФИО в род. падеже: ' + input('signer_name_gen').value);
expect(input('signer_post_gen').value === 'генерального директора', 'должность: ' + input('signer_post_gen').value);
expect(input('kpp').classList.contains('filled'), 'КПП не подсвечен');
expect(input('city').value === '', 'DaData тронул поле объекта');

input('bik').value = '044525225';
lookup('bank').click();
await sleep(600);
expect(input('bank').value === 'ПАО «Сбербанк»', 'банк: ' + input('bank').value);
expect(input('corr_account').value === '30101810400000000225', 'корр. счёт: ' + input('corr_account').value);

btn('Сохранить').click();
await sleep(600);
expect(/^#\/landlords\/\d+$/.test(location.hash), 'не сохранилось: ' + location.hash + ' ' + message().textContent);
expect(document.querySelectorAll('.filled').length === 0, 'подсветка осталась после сохранения');

location.hash = '#/tenants/new';
await sleep(600);
input('inn').value = '500100732259';
lookup('party').click();
await sleep(600);
expect(input('name').value === 'Сидорова Анна Сергеевна', 'ФИО ИП: ' + input('name').value);
expect(input('gender').value === 'f', 'пол ИП: ' + input('gender').value);
expect(input('ogrnip').value === '304500116000157', 'ОГРНИП: ' + input('ogrnip').value);

input('kind').value = 'person';
input('kind').dispatchEvent(new Event('change'));
await sleep(100);
expect(!lookup('party'), 'у физлица есть кнопка «Заполнить по ИНН»');
expect(lookup('bank'), 'у физлица нет кнопки «Заполнить по БИК»');
log.push('ИП заполнен, у физлица только БИК');
