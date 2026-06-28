#!/usr/bin/env ruby
# remover.rb
# encoding: UTF-8

require 'optparse'
require 'find'
require 'fileutils'

COLORS = {
  green: "\e[92m",
  red: "\e[91m",
  yellow: "\e[93m",
  blue: "\e[94m",
  reset: "\e[0m"
}

def colorize(text, color)
  "#{COLORS[color]}#{text}#{COLORS[:reset]}"
end

def is_ignored?(name, ignore_set)
  ignore_set.include?(name)
end

def is_folder_empty?(dir, ignore_set, min_size)
  return false unless Dir.exist?(dir)
  entries = Dir.entries(dir) - ['.', '..']
  filtered = entries.reject { |e| is_ignored?(e, ignore_set) }
  return true if filtered.empty?
  if min_size && min_size > 0
    all_small = true
    filtered.each do |f|
      full = File.join(dir, f)
      if File.directory?(full)
        all_small = false
        break
      end
      if File.size(full) >= min_size
        all_small = false
        break
      end
    end
    return true if all_small
  end
  false
end

def get_empty_dirs(root, recursive, exclude_patterns, ignore_set, min_size)
  result = []
  Find.find(root) do |path|
    next unless File.directory?(path)
    # Исключения
    skip = false
    exclude_patterns.each do |pat|
      if File.fnmatch(pat, File.basename(path))
        skip = true
        break
      end
    end
    next if skip
    if is_folder_empty?(path, ignore_set, min_size)
      result << path
    end
    Find.prune if !recursive && path != root
  end
  # Сортировка по глубине
  result.sort_by! { |p| -p.count(File::SEPARATOR) }
  result
end

options = {
  path: '.',
  recursive: true,
  dry_run: false,
  verbose: false,
  log: nil,
  exclude: [],
  ignore_files: [],
  min_size: 0,
  yes: false
}

parser = OptionParser.new do |opts|
  opts.banner = "Usage: remover.rb [options]"
  opts.on("-p", "--path DIR", "Корневая папка") { |v| options[:path] = v }
  opts.on("-r", "--recursive", "Рекурсивно") { options[:recursive] = true }
  opts.on("-n", "--dry-run", "Симуляция") { options[:dry_run] = true }
  opts.on("-v", "--verbose", "Подробно") { options[:verbose] = true }
  opts.on("-l", "--log FILE", "Лог-файл") { |v| options[:log] = v }
  opts.on("-e", "--exclude PAT", "Исключить (glob)") { |v| options[:exclude] << v }
  opts.on("-i", "--ignore-files LIST", "Игнорируемые файлы (через запятую)") { |v| options[:ignore_files] = v.split(',').map(&:strip) }
  opts.on("-m", "--min-size N", Integer, "Минимальный размер (байты)") { |v| options[:min_size] = v }
  opts.on("-y", "--yes", "Не запрашивать подтверждение") { options[:yes] = true }
  opts.on("-h", "--help", "Справка") { puts opts; exit }
end
parser.parse!

root = File.expand_path(options[:path])
unless Dir.exist?(root)
  puts colorize("Ошибка: '#{root}' не является папкой", :red)
  exit 1
end

default_ignore = %w[.DS_Store Thumbs.db desktop.ini]
ignore_set = Set.new(options[:ignore_files] + default_ignore)

empty_dirs = get_empty_dirs(root, options[:recursive], options[:exclude], ignore_set, options[:min_size])
if empty_dirs.empty?
  puts colorize("Пустых папок не найдено.", :yellow)
  exit 0
end

puts colorize("Найдено #{empty_dirs.size} пустых папок:", :blue)
if options[:verbose]
  empty_dirs.each { |d| puts "  #{d}" }
end

if options[:dry_run]
  puts colorize("Режим симуляции. Ничего не удалено.", :green)
  exit 0
end

unless options[:yes]
  print colorize("Удалить #{empty_dirs.size} папок? [y/N] ", :yellow)
  ans = $stdin.gets.chomp
  unless ans.downcase == 'y'
    puts colorize("Операция отменена.", :red)
    exit 0
  end
end

log_lines = []
deleted = 0
empty_dirs.each do |d|
  if options[:verbose]
    puts colorize("Удаление: #{d}", :green)
  end
  begin
    FileUtils.rm_rf(d)
    deleted += 1
    log_lines << d
  rescue => e
    puts colorize("Ошибка удаления #{d}: #{e.message}", :red)
  end
end

if options[:log]
  begin
    File.write(options[:log], log_lines.join("\n"))
    puts colorize("Лог сохранён в #{options[:log]}", :green)
  rescue => e
    puts colorize("Ошибка записи лога: #{e.message}", :red)
  end
end

puts colorize("Удалено #{deleted} папок.", :green)
