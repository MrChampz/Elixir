from pathlib import Path
exec(Path(__file__).with_name('parse-aether-trace.py').read_text().split('for variant in sys.argv')[0])
import csv

def stats(v):
 return {'n':len(v),'mean':statistics.mean(v),'median':statistics.median(v),'p95':sorted(v)[int(.95*(len(v)-1))]}
for variant in sys.argv[1:]:
 path='/tmp/aether-'+variant
 if not Path(path+'-time-profile.xml').exists(): continue
 frames,res=read(path+'-ca-client-present-request.xml');times=[float(row['timestamp'].text)/1e6 for row in frames if row['process'].attrib.get('fmt','').startswith('Dissolve (')]
 dt=[b-a for a,b in zip(times,times[1:])];result={'version':variant,'frames':len(times),'frame_intervals_ms':stats(dt),'coverage_s':(times[-1]-times[0])/1000}
 gpu,res=read(path+'-metal-gpu-intervals.xml');groups=defaultdict(lambda:defaultdict(list))
 for row in gpu:
  if not row['process'].attrib.get('fmt','').startswith('Dissolve ('):continue
  if row['event-depth'].text!='0' or row['state'].text!='Active':continue
  groups[row['channel-name'].text][row['cmdbuffer-id'].text].append((float(row['start'].text)/1e6,float(row['duration'].text)/1e6))
 compute={k:sorted(v) for k,v in groups['Compute'].items() if len(v)==6}
 result['compute_ms']=stats([sum(d for _,d in v) for v in compute.values()])
 result['compute_encoder_order_ms']=[stats([v[i][1] for v in compute.values()]) for i in range(6)]
 result['vertex_ms_per_present']=sum(d for v in groups['Vertex'].values() for _,d in v)/len(times)
 result['fragment_ms_per_present']=sum(d for v in groups['Fragment'].values() for _,d in v)/len(times)
 profile,res=read(path+'-time-profile.xml');cpu=0;sim=0;manager=0
 for row in profile:
  if not row['process'].attrib.get('fmt','').startswith('Dissolve ('):continue
  weight=float(row['weight'].text or 0)/1e6;cpu+=weight;stack=res(row['stack']);bt=stack.find('backtrace')
  if bt is None:continue
  names={res(f).attrib.get('name','') for f in res(bt)}
  if any('Aether::Simulation::Simulator::Simulate(' in x for x in names):sim+=weight
  if any('Aether::Manager::Render(' in x for x in names):manager+=weight
 result['cpu_sample_ms_per_present']=cpu/len(times);result['simulator_cpu_sample_ms_per_present']=sim/len(times);result['aether_render_cpu_sample_ms_per_present']=manager/len(times)
 result['simulator_sample_count']=sim
 Path(path+'-metrics.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))
 with open(path+'-compute.csv','w') as out:
  w=csv.writer(out);w.writerow(['command_buffer','start_ms','compute_ms','encoder_0_ms','encoder_1_ms','encoder_2_ms','encoder_3_ms','encoder_4_ms','encoder_5_ms'])
  for key,v in compute.items():w.writerow([key,v[0][0],sum(d for _,d in v),*[d for _,d in v]])
