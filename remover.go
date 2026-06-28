// remover.go
package main

import (
	"bufio"
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"sort"
	"strings"
)

const (
	reset  = "\033[0m"
	green  = "\033[92m"
	red    = "\033[91m"
	yellow = "\033[93m"
	blue   = "\033[94m"
)

func colorize(text, color string) string {
	return color + text + reset
}

func isIgnored(name string, ignoreList []string) bool {
	for _, ig := range ignoreList {
		if name == ig {
			return true
		}
	}
	return false
}

func isFolderEmpty(dir string, ignoreList []string, minSize int64) bool {
	entries, err := os.ReadDir(dir)
	if err != nil {
		return false
	}
	var filtered []os.DirEntry
	for _, e := range entries {
		if !isIgnored(e.Name(), ignoreList) {
			filtered = append(filtered, e)
		}
	}
	if len(filtered) == 0 {
		return true
	}
	if minSize > 0 {
		allSmall := true
		for _, e := range filtered {
			info, err := e.Info()
			if err != nil {
				allSmall = false
				break
			}
			if info.IsDir() {
				allSmall = false
				break
			}
			if info.Size() >= minSize {
				allSmall = false
				break
			}
		}
		if allSmall {
			return true
		}
	}
	return false
}

func getEmptyDirs(root string, recursive bool, exclude []string, ignoreList []string, minSize int64) ([]string, error) {
	var dirs []string
	err := filepath.WalkDir(root, func(path string, d os.DirEntry, err error) error {
		if err != nil {
			return err
		}
		if !d.IsDir() {
			return nil
		}
		// Исключения
		base := filepath.Base(path)
		for _, pat := range exclude {
			matched, err := filepath.Match(pat, base)
			if err != nil {
				return err
			}
			if matched {
				return filepath.SkipDir
			}
		}
		// Проверяем пустоту
		if isFolderEmpty(path, ignoreList, minSize) {
			dirs = append(dirs, path)
		}
		if !recursive && path != root {
			return filepath.SkipDir
		}
		return nil
	})
	if err != nil {
		return nil, err
	}
	// Сортировка по глубине (сначала глубокие)
	sort.Slice(dirs, func(i, j int) bool {
		return strings.Count(dirs[i], string(os.PathSeparator)) > strings.Count(dirs[j], string(os.PathSeparator))
	})
	return dirs, nil
}

func main() {
	var (
		root        string
		recursive   bool
		dryRun      bool
		verbose     bool
		logFile     string
		excludeStr  string
		ignoreStr   string
		minSize     int64
		yes         bool
	)
	flag.StringVar(&root, "p", ".", "Корневая папка")
	flag.BoolVar(&recursive, "r", true, "Рекурсивно")
	flag.BoolVar(&dryRun, "n", false, "Симуляция")
	flag.BoolVar(&verbose, "v", false, "Подробно")
	flag.StringVar(&logFile, "l", "", "Лог-файл")
	flag.StringVar(&excludeStr, "e", "", "Исключить (glob-шаблоны через запятую)")
	flag.StringVar(&ignoreStr, "i", "", "Игнорируемые файлы (через запятую)")
	flag.Int64Var(&minSize, "m", 0, "Минимальный размер (байты)")
	flag.BoolVar(&yes, "y", false, "Не запрашивать подтверждение")
	flag.Parse()

	root, err := filepath.Abs(root)
	if err != nil {
		fmt.Println(colorize("Ошибка получения абсолютного пути: "+err.Error(), red))
		os.Exit(1)
	}
	info, err := os.Stat(root)
	if err != nil || !info.IsDir() {
		fmt.Println(colorize("Ошибка: '"+root+"' не является папкой", red))
		os.Exit(1)
	}

	var exclude []string
	if excludeStr != "" {
		exclude = strings.Split(excludeStr, ",")
	}
	var ignoreList []string
	if ignoreStr != "" {
		ignoreList = strings.Split(ignoreStr, ",")
	}
	// Добавляем системные
	defaultIgnore := []string{".DS_Store", "Thumbs.db", "desktop.ini"}
	ignoreMap := make(map[string]bool)
	for _, s := range ignoreList {
		ignoreMap[strings.TrimSpace(s)] = true
	}
	for _, s := range defaultIgnore {
		ignoreMap[s] = true
	}
	var finalIgnore []string
	for k := range ignoreMap {
		finalIgnore = append(finalIgnore, k)
	}

	emptyDirs, err := getEmptyDirs(root, recursive, exclude, finalIgnore, minSize)
	if err != nil {
		fmt.Println(colorize("Ошибка сканирования: "+err.Error(), red))
		os.Exit(1)
	}

	if len(emptyDirs) == 0 {
		fmt.Println(colorize("Пустых папок не найдено.", yellow))
		return
	}

	fmt.Println(colorize(fmt.Sprintf("Найдено %d пустых папок:", len(emptyDirs)), blue))
	if verbose {
		for _, d := range emptyDirs {
			fmt.Printf("  %s\n", d)
		}
	}

	if dryRun {
		fmt.Println(colorize("Режим симуляции. Ничего не удалено.", green))
		return
	}

	if !yes {
		fmt.Print(colorize(fmt.Sprintf("Удалить %d папок? [y/N] ", len(emptyDirs)), yellow))
		reader := bufio.NewReader(os.Stdin)
		ans, _ := reader.ReadString('\n')
		if strings.ToLower(strings.TrimSpace(ans)) != "y" {
			fmt.Println(colorize("Операция отменена.", red))
			return
		}
	}

	logLines := []string{}
	deleted := 0
	for _, d := range emptyDirs {
		if verbose {
			fmt.Println(colorize("Удаление: "+d, green))
		}
		if err := os.RemoveAll(d); err != nil {
			fmt.Println(colorize("Ошибка удаления "+d+": "+err.Error(), red))
		} else {
			deleted++
			logLines = append(logLines, d)
		}
	}

	if logFile != "" {
		if err := os.WriteFile(logFile, []byte(strings.Join(logLines, "\n")), 0644); err != nil {
			fmt.Println(colorize("Ошибка записи лога: "+err.Error(), red))
		} else {
			fmt.Println(colorize("Лог сохранён в "+logFile, green))
		}
	}

	fmt.Println(colorize(fmt.Sprintf("Удалено %d папок.", deleted), green))
}
