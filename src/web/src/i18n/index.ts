// i18n 初始化（决策 40：react-i18next，默认中文，key 化管理）。
// 英文包 TODO；语言资源内联打包（无网络请求）。

import i18n from 'i18next';
import { initReactI18next } from 'react-i18next';

import { zh } from './zh';

void i18n.use(initReactI18next).init({
  resources: {
    zh: { translation: zh },
  },
  lng: 'zh',
  fallbackLng: 'zh',
  interpolation: { escapeValue: false },
});

export default i18n;
