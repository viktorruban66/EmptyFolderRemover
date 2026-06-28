// remover.js
#!/usr/bin/env node
'use strict';

const fs = require('fs');
const path = require('path');
const { promisify } = require('util');
const readdir = promisify(fs.readdir);
const rmdir = promisify(fs.rmdir);
const stat = promisify(fs.stat);
const glob = require('glob'); // npm install glob

const COLORS = {
    green: '\x1b[92m',
    red: '\x1b[91m',
    yellow: '\x1b[93m',
    blue: '\x1b[94m',
    reset: '\x1b[0m'
};

function colorize(text, color) {
    return COLORS[color] + text + COLORS.reset;
}

function isIgnored(name, ignoreList) {
    return ignoreList.includes(name);
}

async function isFolderEmpty(dir, ignoreFiles, minSize) {
    try {
        const items = await readdir(dir);
        let filtered = items.filter(f => !ignoreFiles.includes(f));
        if (filtered.length === 0) return true;

        // Проверка размера
        if (minSize && minSize > 0) {
            let allSmall = true;
            for (const f of filtered) {
                const full = path.join(dir, f);
                const st = await stat(full);
                if (st.isDirectory()) { allSmall = false; break; }
                if (st.size >= minSize) { allSmall = false; break; }
            }
            if (allSmall) return true;
        }
        return false;
    } catch (err) {
        return false;
    }
}

async function getEmptyDirs(root, recursive, exclude, ignoreFiles, minSize) {
    const result = [];
    const walk = async (dir) => {
        const items = await readdir(dir);
        for (const item of items) {
            const full = path.join(dir, item);
            const st = await stat(full);
            if (st.isDirectory()) {
                if (recursive) {
                    await walk(full);
                }
                // Проверка исключений
                let skip = false;
                for (const pat of exclude) {
                    if (glob.sync(pat, { cwd: path.dirname(full), strict: false }).includes(item)) {
                        skip = true;
                        break;
                    }
                }
                if (skip) continue;
                const empty = await isFolderEmpty(full, ignoreFiles, minSize);
                if (empty) {
                    result.push(full);
                }
            }
        }
    };
    await walk(root);
    // Сортируем по глубине (сначала самые глубокие)
    result.sort((a, b) => b.split(path.sep).length - a.split(path.sep).length);
    return result;
}

async function main() {
    const args = require('minimist')(process.argv.slice(2), {
        string: ['p', 'l', 'e', 'i'],
        boolean: ['r', 'n', 'v', 'y'],
        alias: { p: 'path', r: 'recursive', n: 'dry-run', v: 'verbose', l: 'log', e: 'exclude', i: 'ignore-files', m: 'min-size', y: 'yes' },
        default: { p: '.', r: true, 'min-size': 0 }
    });

    const root = path.resolve(args.p);
    if (!fs.existsSync(root) || !fs.statSync(root).isDirectory()) {
        console.log(colorize(`Ошибка: '${root}' не является папкой`, 'red'));
        process.exit(1);
    }

    const recursive = args.r;
    const dryRun = args.n;
    const verbose = args.v;
    const logFile = args.l;
    const exclude = args.e ? (Array.isArray(args.e) ? args.e : [args.e]) : [];
    const ignoreFiles = args.i ? args.i.split(',').map(s => s.trim()) : [];
    const minSize = parseInt(args['min-size']) || 0;
    const yes = args.y;

    // Добавляем системные файлы
    const defaultIgnore = ['.DS_Store', 'Thumbs.db', 'desktop.ini'];
    const ignoreSet = new Set([...ignoreFiles, ...defaultIgnore]);
    const ignoreList = Array.from(ignoreSet);

    const emptyDirs = await getEmptyDirs(root, recursive, exclude, ignoreList, minSize);
    if (emptyDirs.length === 0) {
        console.log(colorize('Пустых папок не найдено.', 'yellow'));
        return;
    }

    console.log(colorize(`Найдено ${emptyDirs.length} пустых папок:`, 'blue'));
    if (verbose) {
        emptyDirs.forEach(d => console.log(`  ${d}`));
    }

    if (dryRun) {
        console.log(colorize('Режим симуляции. Ничего не удалено.', 'green'));
        return;
    }

    if (!yes) {
        const answer = await new Promise(resolve => {
            const rl = require('readline').createInterface({
                input: process.stdin,
                output: process.stdout
            });
            rl.question(colorize(`Удалить ${emptyDirs.length} папок? [y/N] `, 'yellow'), ans => {
                rl.close();
                resolve(ans);
            });
        });
        if (answer.toLowerCase() !== 'y') {
            console.log(colorize('Операция отменена.', 'red'));
            return;
        }
    }

    const logLines = [];
    let deleted = 0;
    for (const d of emptyDirs) {
        try {
            if (verbose) console.log(colorize(`Удаление: ${d}`, 'green'));
            // Рекурсивное удаление (rmdir с опцией recursive для Node >= 14)
            await rmdir(d, { recursive: true });
            deleted++;
            logLines.push(d);
        } catch (err) {
            console.log(colorize(`Ошибка при удалении ${d}: ${err.message}`, 'red'));
        }
    }

    if (logFile) {
        try {
            fs.writeFileSync(logFile, logLines.join('\n'), 'utf8');
            console.log(colorize(`Лог сохранён в ${logFile}`, 'green'));
        } catch (err) {
            console.log(colorize(`Ошибка записи лога: ${err.message}`, 'red'));
        }
    }

    console.log(colorize(`Удалено ${deleted} папок.`, 'green'));
}

main().catch(err => {
    console.log(colorize(`Ошибка: ${err.message}`, 'red'));
    process.exit(1);
});
