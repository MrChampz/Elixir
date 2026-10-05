import xml.etree.ElementTree as E
from pathlib import Path
from collections import Counter, defaultdict
import statistics, json, sys

def read(path):
 r=E.parse(path).getroot(); ids={e.attrib['id']:e for e in r.iter() if 'id' in e.attrib}
 def deref(e): return ids[e.attrib['ref']] if 'ref' in e.attrib else e
 columns=[c.findtext('mnemonic') for c in r.findall('.//schema/col')]
 return [{c:deref(e) for c,e in zip(columns,row)} for row in r.findall('.//row')], deref

for variant in sys.argv[1:]:
 data={}
 p=Path('/tmp/aether-'+variant+'-ca-client-present-request.xml')
 if not p.exists(): continue
 frames,res=read(p); times=[float(row['timestamp'].text)/1e6 for row in frames]
 dt=[b-a for a,b in zip(times,times[1:])]
 data['present_requests']=len(times);data['window_ms']=times[-1]-times[0] if times else 0
 if dt:data['frame_interval_ms']={'mean':statistics.mean(dt),'median':statistics.median(dt),'p95':sorted(dt)[int(.95*(len(dt)-1))]}
 gpu,res=read('/tmp/aether-'+variant+'-metal-gpu-intervals.xml')
 groups=defaultdict(list)
 for row in gpu:
  if not row['process'].attrib.get('fmt','').startswith('Dissolve ('):continue
  if row['event-depth'].text!='0' or row['state'].text!='Active':continue
  groups[row['channel-name'].text].append(float(row['duration'].text)/1e6)
 data['gpu_channel_intervals']={k:{'count':len(v),'total_ms':sum(v),'mean_ms':statistics.mean(v)} for k,v in groups.items()}
 profile,res=read('/tmp/aether-'+variant+'-time-profile.xml')
 inclusive=Counter();total=0
 for row in profile:
  weight=float(row['weight'].text or 0)/1e6;total+=weight
  stack=res(row['stack']); backtrace=stack.find('backtrace')
  if backtrace is None:continue
  seen=set()
  for f in res(backtrace):
   name=res(f).attrib.get('name','')
   if name not in seen: inclusive[name]+=weight;seen.add(name)
 data['cpu_sample_ms']=total;data['cpu_inclusive_ms']=inclusive.most_common(35)
 print(variant,json.dumps(data,indent=2))
 Path('/tmp/aether-'+variant+'-summary.json').write_text(json.dumps(data,indent=2))
