// Арендодатели: создать через форму, ошибка ИНН показывается и ничего не сохраняет,
// изменить поле, найти в списке, удалить.
const fill = values => {
    for (const [name, value] of Object.entries(values)) {
        document.querySelector(`form.card input[name=${name}]`).value = value;
    }
};
const message = () => document.getElementById('message');

document.querySelector('a.button').click();
await sleep(500);
expect(location.hash === '#/landlords/new', 'не открылась форма: ' + location.hash);
expect(document.querySelectorAll('form.card fieldset').length === 6, 'разделов формы: ' + document.querySelectorAll('form.card fieldset').length);
expect(document.querySelector('input[name=contract_limit]').value === '100 000', 'нет порога по умолчанию в новой карточке');

fill({name_full: 'Общество с ограниченной ответственностью «Ромашка»', name_short: 'ООО «Ромашка»', inn: '7707083894'});
btn('Сохранить').click();
await sleep(500);
expect(message().className === 'error' && message().textContent.includes('ИНН «7707083894» с ошибкой'), 'нет ошибки ИНН: ' + message().textContent);
expect(location.hash === '#/landlords/new', 'с ошибкой ушли со страницы: ' + location.hash);
log.push('ошибка ИНН: ' + message().textContent);

fill({inn: '7707083893', city: 'г. Химки'});
btn('Сохранить').click();
await sleep(600);
expect(/^#\/landlords\/\d+$/.test(location.hash), 'после сохранения не открылась карточка: ' + location.hash);
expect(document.querySelector('h2').textContent === 'ООО «Ромашка»', 'заголовок: ' + document.querySelector('h2').textContent);
expect(message().className === 'ok' && message().textContent === 'Сохранено: ООО «Ромашка»',
    'после перехода на карточку пропало сообщение: «' + message().textContent + '»');
expect(document.querySelector('input[name=city]').value === 'г. Химки', 'город не сохранился');

fill({name_short: 'ООО «Ромашка-2»'});
btn('Сохранить').click();
await sleep(500);
expect(message().className === 'ok', 'изменение не сохранилось: ' + message().textContent);
expect(document.querySelector('h2').textContent === 'ООО «Ромашка-2»', 'заголовок не обновился');

location.hash = '#/landlords';
await sleep(500);
const rows = [...document.querySelectorAll('table.list tbody tr')].map(r => r.cells[0].textContent + ' / ' + r.cells[1].textContent);
log.push('список: ' + rows.join('; '));
expect(rows.join('; ') === 'ООО «Ромашка-2» / 7707083893', 'в списке: ' + rows.join('; '));

document.querySelector('table.list a').click();
await sleep(500);
btn('Удалить').click();
await sleep(600);
expect(location.hash === '#/landlords', 'после удаления не вернулись к списку: ' + location.hash);
expect(document.querySelector('main').textContent.includes('Арендодателей пока нет'), 'арендодатель не удалён');
