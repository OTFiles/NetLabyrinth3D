import os
import argparse
import glob
from pathlib import Path

def find_files(suffix, recursive=True):
    """查找指定后缀的文件"""
    pattern = f"**/*{suffix}" if recursive else f"*{suffix}"
    return [f for f in glob.glob(pattern, recursive=recursive) if os.path.isfile(f)]

def read_file_content(filepath):
    """读取文件内容"""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            return f.read()
    except UnicodeDecodeError:
        # 如果UTF-8解码失败，尝试其他编码
        try:
            with open(filepath, 'r', encoding='gbk') as f:
                return f.read()
        except:
            return f"[错误：无法读取文件 {filepath}，可能不是文本文件]"
    except Exception as e:
        return f"[错误：读取文件 {filepath} 时发生错误 - {str(e)}]"

def main():
    parser = argparse.ArgumentParser(description='将指定后缀的文件内容合并到一个文件中')
    parser.add_argument('suffix', help='要搜索的文件后缀，例如 .txt 或 .py')
    parser.add_argument('output', help='输出文件名')
    parser.add_argument('-e', '--exclude', nargs='+', help='要排除的文件名列表', default=[])
    parser.add_argument('--no-recursive', action='store_true', help='不搜索子目录')
    
    args = parser.parse_args()
    
    # 查找文件
    files = find_files(args.suffix, not args.no_recursive)
    
    if not files:
        print(f"未找到后缀为 {args.suffix} 的文件")
        return
    
    # 过滤掉排除的文件
    filtered_files = [f for f in files if os.path.basename(f) not in args.exclude]
    
    if not filtered_files:
        print("所有文件都已被排除")
        return
    
    # 按文件名排序
    filtered_files.sort()
    
    # 写入输出文件
    try:
        with open(args.output, 'w', encoding='utf-8') as outfile:
            for filepath in filtered_files:
                filename = os.path.basename(filepath)
                content = read_file_content(filepath)
                
                outfile.write(f"# {filename}\n")
                outfile.write(content)
                outfile.write("\n")  # 在文件之间添加空行
                
        print(f"成功将 {len(filtered_files)} 个文件合并到 {args.output}")
        
    except Exception as e:
        print(f"写入输出文件时发生错误: {str(e)}")

if __name__ == "__main__":
    main()