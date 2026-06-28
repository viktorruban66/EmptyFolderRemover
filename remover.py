# remover.py
#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sys
import argparse
import fnmatch
import shutil
from pathlib import Path

# ANSI colors
COLORS = {
    'green': '\033[92m',
    'red': '\033[91m',
    'yellow': '\033[93m',
    'blue': '\033[94m',
    'reset': '\033[0m'
}

def colorize(text, color):
    return f"{COLORS.get(color, '')}{text}{COLORS['reset']}"

def is_empty(path, ignore_files=None, min_size=None):
    """Проверяет, является ли папка пустой (с учётом игнорируемых файлов и минимального размера)."""
    ignore_files = ignore_files or []
    try:
        items = os.listdir(path)
    except PermissionError:
        return False
    # Фильтруем игнорируемые файлы
    filtered = [f for f in items if f not in ignore_files]
    if not filtered:
        return True  # все файлы игнорируются или папка пуста

    # Проверяем размер файлов
    if min_size is not None and min_size > 0:
        all_small = True
        for f in filtered:
            full = os.path.join(path, f)
            if os.path.isdir(full):
                all_small = False
                break
            try:
                size = os.path.getsize(full)
                if size >= min_size:
                    all_small = False
                    break
            except OSError:
                all_small = False
                break
        if all_small:
            return True
    return False

def get_empty_dirs(root, recursive=True, exclude=None, ignore_files=None, min_size=None):
    """Собирает список пустых папок (снизу вверх)."""
    exclude = exclude or []
    ignore_files = ignore_files or []
    empty_dirs = []
    if recursive:
        for dirpath, dirnames, filenames in os.walk(root, topdown=False):
            # Проверяем исключения
            skip = False
            for pattern in exclude:
                if fnmatch.fnmatch(os.path.basename(dirpath), pattern):
                    skip = True
                    break
            if skip:
                continue
            if is_empty(dirpath, ignore_files, min_size):
                empty_dirs.append(dirpath)
    else:
        # Только корневая папка
        if is_empty(root, ignore_files, min_size):
            empty_dirs.append(root)
    return empty_dirs

def main():
    parser = argparse.ArgumentParser(description="EmptyFolderRemover – удаление пустых папок")
    parser.add_argument('-p', '--path', default='.', help='Корневая папка (по умолчанию текущая)')
    parser.add_argument('-r', '--recursive', action='store_true', default=True,
                        help='Рекурсивно (включено по умолчанию)')
    parser.add_argument('-n', '--dry-run', action='store_true', help='Только показать, что будет удалено')
    parser.add_argument('-v', '--verbose', action='store_true', help='Подробный вывод')
    parser.add_argument('-l', '--log', help='Файл для сохранения лога')
    parser.add_argument('-e', '--exclude', action='append', default=[],
                        help='Исключить папки по glob-шаблону (можно указать несколько)')
    parser.add_argument('-i', '--ignore-files', default='',
                        help='Список файлов для игнорирования (через запятую), например .DS_Store,Thumbs.db')
    parser.add_argument('-m', '--min-size', type=int, default=0,
                        help='Удалять папку, если все файлы меньше указанного размера (в байтах)')
    parser.add_argument('-y', '--yes', action='store_true', help='Не запрашивать подтверждение')
    args = parser.parse_args()

    root = os.path.abspath(args.path)
    if not os.path.isdir(root):
        sys.exit(colorize(f"Ошибка: '{root}' не является папкой", 'red'))

    ignore_list = [x.strip() for x in args.ignore_files.split(',') if x.strip()]
    # Добавляем системные файлы по умолчанию
    default_ignore = ['.DS_Store', 'Thumbs.db', 'desktop.ini']
    ignore_list = list(set(ignore_list + default_ignore))

    try:
        empty_dirs = get_empty_dirs(root, args.recursive, args.exclude,
                                    ignore_list, args.min_size)
    except Exception as e:
        sys.exit(colorize(f"Ошибка при сканировании: {e}", 'red'))

    if not empty_dirs:
        print(colorize("Пустых папок не найдено.", 'yellow'))
        return

    # Сортируем по глубине (сначала самые глубокие)
    empty_dirs.sort(key=lambda p: p.count(os.sep), reverse=True)

    print(colorize(f"Найдено {len(empty_dirs)} пустых папок:", 'blue'))
    if args.verbose:
        for d in empty_dirs:
            print(f"  {d}")

    if args.dry_run:
        print(colorize("Режим симуляции. Ничего не удалено.", 'green'))
        return

    if not args.yes:
        answer = input(colorize(f"Удалить {len(empty_dirs)} папок? [y/N] ", 'yellow'))
        if answer.lower() != 'y':
            print(colorize("Операция отменена.", 'red'))
            return

    log_lines = []
    deleted = 0
    for d in empty_dirs:
        try:
            if args.verbose:
                print(colorize(f"Удаление: {d}", 'green'))
            shutil.rmtree(d)
            deleted += 1
            log_lines.append(d)
        except Exception as e:
            print(colorize(f"Ошибка при удалении {d}: {e}", 'red'))

    if args.log:
        try:
            with open(args.log, 'w', encoding='utf-8') as f:
                f.write('\n'.join(log_lines))
            print(colorize(f"Лог сохранён в {args.log}", 'green'))
        except Exception as e:
            print(colorize(f"Ошибка записи лога: {e}", 'red'))

    print(colorize(f"Удалено {deleted} папок.", 'green'))

if __name__ == '__main__':
    main()
