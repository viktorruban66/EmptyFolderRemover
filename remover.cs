// remover.cs
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;

class Remover
{
    static string Colorize(string text, string color)
    {
        string col = color switch
        {
            "green" => "\x1b[92m",
            "red" => "\x1b[91m",
            "yellow" => "\x1b[93m",
            "blue" => "\x1b[94m",
            _ => "\x1b[0m"
        };
        return col + text + "\x1b[0m";
    }

    static bool IsIgnored(string name, HashSet<string> ignoreSet)
    {
        return ignoreSet.Contains(name);
    }

    static bool IsFolderEmpty(string dir, HashSet<string> ignoreSet, long minSize)
    {
        if (!Directory.Exists(dir)) return false;
        var entries = Directory.GetFileSystemEntries(dir);
        var filtered = entries.Where(f => !ignoreSet.Contains(Path.GetFileName(f))).ToList();
        if (filtered.Count == 0) return true;
        if (minSize > 0)
        {
            bool allSmall = true;
            foreach (var f in filtered)
            {
                if (Directory.Exists(f))
                {
                    allSmall = false;
                    break;
                }
                var fi = new FileInfo(f);
                if (fi.Length >= minSize)
                {
                    allSmall = false;
                    break;
                }
            }
            if (allSmall) return true;
        }
        return false;
    }

    static List<string> GetEmptyDirs(string root, bool recursive, List<string> exclude, HashSet<string> ignoreSet, long minSize)
    {
        var result = new List<string>();
        var dirs = new Stack<string>();
        dirs.Push(root);
        while (dirs.Count > 0)
        {
            string current = dirs.Pop();
            try
            {
                var sub = Directory.GetDirectories(current);
                foreach (var d in sub)
                {
                    if (recursive) dirs.Push(d);
                    // Исключения
                    bool skip = false;
                    foreach (var pat in exclude)
                    {
                        if (System.Text.RegularExpressions.Regex.IsMatch(Path.GetFileName(d), WildcardToRegex(pat)))
                        {
                            skip = true;
                            break;
                        }
                    }
                    if (skip) continue;
                    if (IsFolderEmpty(d, ignoreSet, minSize))
                    {
                        result.Add(d);
                    }
                }
            }
            catch (Exception) { }
        }
        // Сортировка по глубине
        result.Sort((a, b) => b.Count(c => c == Path.DirectorySeparatorChar) - a.Count(c => c == Path.DirectorySeparatorChar));
        return result;
    }

    static string WildcardToRegex(string pattern)
    {
        return "^" + Regex.Escape(pattern).Replace("\\*", ".*").Replace("\\?", ".") + "$";
    }

    static void Main(string[] args)
    {
        string root = ".";
        bool recursive = true, dryRun = false, verbose = false, yes = false;
        string logFile = null;
        var exclude = new List<string>();
        var ignoreList = new List<string>();
        long minSize = 0;

        for (int i = 0; i < args.Length; i++)
        {
            string arg = args[i];
            if (arg == "-p" && i+1 < args.Length) root = args[++i];
            else if (arg == "-r") recursive = true;
            else if (arg == "-n") dryRun = true;
            else if (arg == "-v") verbose = true;
            else if (arg == "-l" && i+1 < args.Length) logFile = args[++i];
            else if (arg == "-e" && i+1 < args.Length) exclude.AddRange(args[++i].Split(','));
            else if (arg == "-i" && i+1 < args.Length) ignoreList.AddRange(args[++i].Split(','));
            else if (arg == "-m" && i+1 < args.Length) minSize = long.Parse(args[++i]);
            else if (arg == "-y") yes = true;
            else if (arg == "-h") { Console.WriteLine("Help..."); return; }
        }

        if (!Directory.Exists(root))
        {
            Console.WriteLine(Colorize($"Ошибка: '{root}' не является папкой", "red"));
            return;
        }

        var defaultIgnore = new HashSet<string> { ".DS_Store", "Thumbs.db", "desktop.ini" };
        foreach (var s in ignoreList) defaultIgnore.Add(s);
        var ignoreSet = defaultIgnore;

        var emptyDirs = GetEmptyDirs(root, recursive, exclude, ignoreSet, minSize);
        if (emptyDirs.Count == 0)
        {
            Console.WriteLine(Colorize("Пустых папок не найдено.", "yellow"));
            return;
        }

        Console.WriteLine(Colorize($"Найдено {emptyDirs.Count} пустых папок:", "blue"));
        if (verbose) emptyDirs.ForEach(d => Console.WriteLine($"  {d}"));

        if (dryRun)
        {
            Console.WriteLine(Colorize("Режим симуляции. Ничего не удалено.", "green"));
            return;
        }

        if (!yes)
        {
            Console.Write(Colorize($"Удалить {emptyDirs.Count} папок? [y/N] ", "yellow"));
            string ans = Console.ReadLine();
            if (ans?.ToLower() != "y")
            {
                Console.WriteLine(Colorize("Операция отменена.", "red"));
                return;
            }
        }

        var logLines = new List<string>();
        int deleted = 0;
        foreach (var d in emptyDirs)
        {
            if (verbose) Console.WriteLine(Colorize($"Удаление: {d}", "green"));
            try
            {
                Directory.Delete(d, true);
                deleted++;
                logLines.Add(d);
            }
            catch (Exception e)
            {
                Console.WriteLine(Colorize($"Ошибка удаления {d}: {e.Message}", "red"));
            }
        }

        if (logFile != null)
        {
            try
            {
                File.WriteAllLines(logFile, logLines);
                Console.WriteLine(Colorize($"Лог сохранён в {logFile}", "green"));
            }
            catch (Exception e)
            {
                Console.WriteLine(Colorize($"Ошибка записи лога: {e.Message}", "red"));
            }
        }

        Console.WriteLine(Colorize($"Удалено {deleted} папок.", "green"));
    }
}
