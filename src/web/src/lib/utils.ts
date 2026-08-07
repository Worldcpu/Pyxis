// cn()：clsx + tailwind-merge 组合（shadcn/ui 基础工具）。

import { clsx, type ClassValue } from 'clsx';
import { twMerge } from 'tailwind-merge';

export function cn(...inputs: ClassValue[]): string {
  return twMerge(clsx(inputs));
}
