// SimBrief 导入页（S9.1 D52）：pilotid 输入 + 导入按钮（loading/双击
// 防护）；XML 拉取失败四分类 Dialog（invalid-pilotid / no-flight-plan /
// network / parse + 常见原因提示）；粘贴 OFP HTML 导入保留（无凭据可用）。

import { ClipboardPaste, Import } from 'lucide-react';
import { useState } from 'react';
import { useTranslation } from 'react-i18next';

import { fetchSimBriefOFP, type SimBriefFailure } from '../../lib/simbrief-api';
import { parseSimBriefOFP, type SimBriefImport } from '../../lib/simbrief-import';
import { Button } from '../ui/button';
import { Input } from '../ui/input';
import { Label } from '../ui/label';
import {
  Dialog,
  DialogContent,
  DialogDescription,
  DialogHeader,
  DialogTitle,
} from '../ui/dialog';

export interface SimBriefTabProps {
  /** 导入成功（XML 或粘贴 HTML 解析结果）→ 页面执行分析/生成链。 */
  onImported: (parsed: SimBriefImport) => void;
  busy: boolean;
}

const FAILURE_KEYS: Record<SimBriefFailure['kind'], string> = {
  'invalid-pilotid': 'simbrief.fail.invalidPilotid',
  'no-flight-plan': 'simbrief.fail.noFlightPlan',
  network: 'simbrief.fail.network',
  parse: 'simbrief.fail.parse',
};

export function SimBriefTab({ onImported, busy }: SimBriefTabProps) {
  const { t } = useTranslation();
  const [pilotid, setPilotid] = useState('');
  const [importing, setImporting] = useState(false);
  const [failure, setFailure] = useState<SimBriefFailure | null>(null);

  const onImport = async () => {
    if (importing || busy) return; // 双击防护
    setImporting(true);
    try {
      const result = await fetchSimBriefOFP(pilotid);
      if (result.ok) {
        onImported(result.data);
      } else {
        setFailure(result.failure);
      }
    } finally {
      setImporting(false);
    }
  };

  const onPaste = () => {
    const html = window.prompt(t('simbrief.pastePrompt'));
    if (!html) return;
    const parsed = parseSimBriefOFP(html);
    if (parsed) onImported(parsed);
    else setFailure({ kind: 'parse' });
  };

  return (
    <div className="space-y-3 p-3">
      <div className="space-y-1">
        <Label className="text-xs text-muted-foreground">
          {t('simbrief.pilotid')}
        </Label>
        <Input
          aria-label={t('simbrief.pilotid')}
          value={pilotid}
          placeholder="123456"
          inputMode="numeric"
          onChange={(e) => setPilotid(e.target.value.replace(/\D/g, ''))}
        />
      </div>
      <Button
        className="w-full"
        onClick={onImport}
        disabled={importing || busy || pilotid.trim() === ''}
      >
        <Import className="h-4 w-4" aria-hidden="true" />
        {importing ? t('simbrief.importing') : t('simbrief.import')}
      </Button>
      <div className="border-t border-border pt-3">
        <p className="mb-1 text-xs text-muted-foreground">
          {t('simbrief.pasteTitle')}
        </p>
        <Button variant="outline" size="sm" className="w-full" onClick={onPaste}>
          <ClipboardPaste className="h-3.5 w-3.5" aria-hidden="true" />
          {t('simbrief.pasteImport')}
        </Button>
        <p className="mt-1 text-[11px] text-muted-foreground">
          {t('simbrief.pasteHint')}
        </p>
      </div>
      <Dialog open={failure !== null} onOpenChange={(o) => !o && setFailure(null)}>
        <DialogContent>
          <DialogHeader>
            <DialogTitle>{t('simbrief.fail.title')}</DialogTitle>
            <DialogDescription>
              {failure && t(FAILURE_KEYS[failure.kind])}
            </DialogDescription>
          </DialogHeader>
          {failure && (
            <p className="text-xs text-muted-foreground">
              {t('simbrief.fail.commonHint')}
            </p>
          )}
        </DialogContent>
      </Dialog>
    </div>
  );
}
