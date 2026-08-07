// 占位页（决策 32：航图/设置模块灰显，后续阶段开放）。

import { useTranslation } from 'react-i18next';

import { Badge } from '../components/ui/badge';

export function ComingSoonPage() {
  const { t } = useTranslation();
  return (
    <div className="flex h-full items-center justify-center">
      <Badge variant="secondary">{t('nav.comingSoon')}</Badge>
    </div>
  );
}
