// remover.java
import java.io.*;
import java.nio.file.*;
import java.nio.file.attribute.*;
import java.util.*;
import java.util.regex.Pattern;

public class remover {
    private static final String RESET = "\u001B[0m";
    private static final String GREEN = "\u001B[92m";
    private static final String RED = "\u001B[91m";
    private static final String YELLOW = "\u001B[93m";
    private static final String BLUE = "\u001B[94m";

    private static String colorize(String text, String color) {
        return color + text + RESET;
    }

    private static boolean isIgnored(String name, Set<String> ignoreSet) {
        return ignoreSet.contains(name);
    }

    private static boolean isFolderEmpty(Path dir, Set<String> ignoreSet, long minSize) throws IOException {
        if (!Files.isDirectory(dir)) return false;
        try (DirectoryStream<Path> stream = Files.newDirectoryStream(dir)) {
            List<Path> entries = new ArrayList<>();
            for (Path entry : stream) entries.add(entry);
            List<Path> filtered = new ArrayList<>();
            for (Path e : entries) {
                if (!isIgnored(e.getFileName().toString(), ignoreSet))
                    filtered.add(e);
            }
            if (filtered.isEmpty()) return true;
            if (minSize > 0) {
                boolean allSmall = true;
                for (Path e : filtered) {
                    if (Files.isDirectory(e)) { allSmall = false; break; }
                    if (Files.size(e) >= minSize) { allSmall = false; break; }
                }
                if (allSmall) return true;
            }
            return false;
        }
    }

    private static List<Path> getEmptyDirs(Path root, boolean recursive, List<String> exclude,
                                           Set<String> ignoreSet, long minSize) throws IOException {
        List<Path> result = new ArrayList<>();
        Files.walkFileTree(root, new SimpleFileVisitor<Path>() {
            @Override
            public FileVisitResult preVisitDirectory(Path dir, BasicFileAttributes attrs) {
                if (!recursive && !dir.equals(root)) return FileVisitResult.SKIP_SUBTREE;
                // Исключения
                for (String pat : exclude) {
                    if (Pattern.matches(pat, dir.getFileName().toString())) {
                        return FileVisitResult.SKIP_SUBTREE;
                    }
                }
                try {
                    if (isFolderEmpty(dir, ignoreSet, minSize)) {
                        result.add(dir);
                    }
                } catch (IOException ignored) {}
                return FileVisitResult.CONTINUE;
            }
        });
        // Сортировка по глубине (сначала глубокие)
        result.sort((a, b) -> b.getNameCount() - a.getNameCount());
        return result;
    }

    public static void main(String[] args) throws IOException {
        String root = ".";
        boolean recursive = true, dryRun = false, verbose = false, yes = false;
        String logFile = null;
        List<String> exclude = new ArrayList<>();
        List<String> ignoreList = new ArrayList<>();
        long minSize = 0;

        for (int i = 0; i < args.length; i++) {
            String arg = args[i];
            if (arg.equals("-p") && i+1 < args.length) root = args[++i];
            else if (arg.equals("-r")) recursive = true;
            else if (arg.equals("-n")) dryRun = true;
            else if (arg.equals("-v")) verbose = true;
            else if (arg.equals("-l") && i+1 < args.length) logFile = args[++i];
            else if (arg.equals("-e") && i+1 < args.length) exclude.addAll(Arrays.asList(args[++i].split(",")));
            else if (arg.equals("-i") && i+1 < args.length) ignoreList.addAll(Arrays.asList(args[++i].split(",")));
            else if (arg.equals("-m") && i+1 < args.length) minSize = Long.parseLong(args[++i]);
            else if (arg.equals("-y")) yes = true;
            else if (arg.equals("-h")) { System.out.println("Help..."); return; }
        }

        Path rootPath = Paths.get(root).toAbsolutePath();
        if (!Files.isDirectory(rootPath)) {
            System.err.println(colorize("Ошибка: '" + root + "' не является папкой", RED));
            System.exit(1);
        }

        Set<String> defaultIgnore = new HashSet<>(Arrays.asList(".DS_Store", "Thumbs.db", "desktop.ini"));
        defaultIgnore.addAll(ignoreList);
        Set<String> ignoreSet = defaultIgnore;

        List<Path> emptyDirs = getEmptyDirs(rootPath, recursive, exclude, ignoreSet, minSize);
        if (emptyDirs.isEmpty()) {
            System.out.println(colorize("Пустых папок не найдено.", YELLOW));
            return;
        }

        System.out.println(colorize("Найдено " + emptyDirs.size() + " пустых папок:", BLUE));
        if (verbose) {
            for (Path d : emptyDirs) System.out.println("  " + d);
        }

        if (dryRun) {
            System.out.println(colorize("Режим симуляции. Ничего не удалено.", GREEN));
            return;
        }

        if (!yes) {
            System.out.print(colorize("Удалить " + emptyDirs.size() + " папок? [y/N] ", YELLOW));
            try (Scanner sc = new Scanner(System.in)) {
                String ans = sc.nextLine();
                if (!ans.equalsIgnoreCase("y")) {
                    System.out.println(colorize("Операция отменена.", RED));
                    return;
                }
            }
        }

        List<String> logLines = new ArrayList<>();
        int deleted = 0;
        for (Path d : emptyDirs) {
            if (verbose) System.out.println(colorize("Удаление: " + d, GREEN));
            try {
                Files.walkFileTree(d, new SimpleFileVisitor<Path>() {
                    @Override
                    public FileVisitResult visitFile(Path file, BasicFileAttributes attrs) throws IOException {
                        Files.delete(file);
                        return FileVisitResult.CONTINUE;
                    }
                    @Override
                    public FileVisitResult postVisitDirectory(Path dir, IOException exc) throws IOException {
                        Files.delete(dir);
                        return FileVisitResult.CONTINUE;
                    }
                });
                deleted++;
                logLines.add(d.toString());
            } catch (IOException e) {
                System.err.println(colorize("Ошибка удаления " + d + ": " + e.getMessage(), RED));
            }
        }

        if (logFile != null) {
            try {
                Files.write(Paths.get(logFile), logLines);
                System.out.println(colorize("Лог сохранён в " + logFile, GREEN));
            } catch (IOException e) {
                System.err.println(colorize("Ошибка записи лога: " + e.getMessage(), RED));
            }
        }

        System.out.println(colorize("Удалено " + deleted + " папок.", GREEN));
    }
}
