// SimBrief 最小导入（决策 39：DOMParser 解析 OFP HTML——起降场/航路串/机型）。
// 优先 input[name=origin/destination/route/aircraft_type]，回退 div#route_*。

export interface SimBriefImport {
  departure: string;
  arrival: string;
  routeString: string;
  airframeType: string;
}

function value(inputs: HTMLCollectionOf<HTMLInputElement>, name: string): string {
  for (let i = 0; i < inputs.length; ++i) {
    if (inputs[i].name === name) return inputs[i].value.trim();
  }
  return '';
}

function textById(doc: Document, id: string): string {
  const el = doc.getElementById(id);
  return el?.textContent?.trim() ?? '';
}

/** 解析 SimBrief OFP HTML；解析失败返回 null（静默，不打断流程）。 */
export function parseSimBriefOFP(html: string): SimBriefImport | null {
  try {
    // 非 HTML（无标签字符）直接拒绝——DOMParser 对纯文本也容错出 html。
    if (!html.includes('<')) return null;
    const doc = new DOMParser().parseFromString(html, 'text/html');
    if (!doc.querySelector('html')) return null;
    const inputs = doc.getElementsByTagName('input');

    const byName = {
      departure: value(inputs, 'origin'),
      arrival: value(inputs, 'destination'),
      routeString: value(inputs, 'route'),
      airframeType: value(inputs, 'aircraft_type'),
    };
    if (byName.departure || byName.arrival || byName.routeString) {
      return byName;
    }
    // 旧格式回退：div#route_origin / route_destination / route_route。
    return {
      departure: textById(doc, 'route_origin'),
      arrival: textById(doc, 'route_destination'),
      routeString: textById(doc, 'route_route'),
      airframeType: '',
    };
  } catch {
    return null;
  }
}
