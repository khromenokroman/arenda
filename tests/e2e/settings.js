// Ключ DaData в «Настройках»: без ключа заполнение не работает, ключ вводится в форме, «Проверить»,
// после этого «Заполнить по ИНН» работает; ключ целиком не показывается; удаление ключа.
const message = () => document.getElementById('message');
expect(document.querySelector('main').textContent.includes('ключ не задан'), 'нет состояния «ключ не задан»');
btn('Проверить').click();
await sleep(600);
expect(message().textContent.includes('не задан ключ DaData'), 'без ключа: ' + message().textContent);

document.querySelector('#dadata [name=key]').value = 'good';
btn('Сохранить ключ').click();
await sleep(600);
expect(message().textContent.startsWith('Ключ DaData сохранён'), 'ключ не сохранён: ' + message().textContent);
expect(document.querySelector('main').textContent.includes('ключ задан здесь'), 'состояние после сохранения');
expect(!document.querySelector('main').innerHTML.includes('good'), 'ключ целиком виден на странице');
btn('Проверить').click();
await sleep(600);
expect(message().className === 'ok' && message().textContent.startsWith('Ключ работает'), 'проверка: ' + message().textContent);

location.hash = '#/landlords/new';
await sleep(600);
document.querySelector('form.card [name=inn]').value = '7707083893';
document.querySelector('button[data-lookup=party]').click();
await sleep(600);
expect(document.querySelector('form.card [name=kpp]').value === '773601001', 'заполнение по ИНН с ключом из «Настроек» не сработало');

location.hash = '#/settings';
await sleep(600);
btn('Удалить ключ').click();
await sleep(600);
expect(document.querySelector('main').textContent.includes('ключ не задан'), 'ключ не удалён');
log.push('ключ сохранён, проверен, заполнение работает, ключ удалён');
