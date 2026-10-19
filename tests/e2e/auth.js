// Вход и роли: без сессии форма входа, неверный пароль, вход администратора, новый пользователь без
// раздела «Пользователи», смена пароля, выход.
const message = () => document.getElementById('message');
const fill = values => {
    for (const [name, value] of Object.entries(values)) {
        document.querySelector(`form [name=${name}]`).value = value;
    }
};
const menu = () => [...document.querySelectorAll('#menu a')].map(a => a.textContent).join(', ');

expect(document.querySelector('h2').textContent === 'Вход', 'нет формы входа: ' + document.querySelector('h2').textContent);
expect(menu() === '', 'без входа видно меню: ' + menu());
fill({login: 'admin', password: 'wrong!'});
btn('Войти').click();
await sleep(1200);
expect(message().textContent.includes('неверный логин или пароль'), 'нет ошибки пароля: ' + message().textContent);

fill({login: 'admin', password: 'secret1'});
btn('Войти').click();
await sleep(1200);
expect(document.querySelector('h2').textContent === 'Договоры', 'после входа не открылись договоры: ' + document.querySelector('h2').textContent);
expect(menu() === 'Договоры, Арендодатели, Арендаторы, Пользователи, Копии', 'меню администратора: ' + menu());

location.hash = '#/users';
await sleep(500);
fill({login: 'olga', name: 'Ольга', password: 'secret2'});
btn('Добавить').click();
await sleep(1200);
expect([...document.querySelectorAll('table.users tbody tr')].some(tr => tr.cells[0].textContent === 'olga'), 'пользователь не добавлен');

document.getElementById('logout').click();
await sleep(500);
expect(document.querySelector('h2').textContent === 'Вход', 'после выхода нет формы входа');
fill({login: 'olga', password: 'secret2'});
btn('Войти').click();
await sleep(1200);
expect(menu() === 'Договоры, Арендодатели, Арендаторы', 'у пользователя меню: ' + menu());
expect(document.getElementById('account').textContent.includes('Ольга'), 'в шапке нет имени');

location.hash = '#/password';
await sleep(400);
fill({old_password: 'secret2', password: 'secret3', password2: 'secret3'});
btn('Сменить пароль').click();
await sleep(1200);
expect(message().className === 'ok', 'пароль не сменён: ' + message().textContent);
const check = await fetch('/api/landlords');
expect(check.status === 200, 'после смены пароля эта вкладка потеряла вход: ' + check.status);
log.push('вход, роли, смена пароля, выход');
