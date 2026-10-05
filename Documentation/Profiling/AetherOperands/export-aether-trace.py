import subprocess,sys
variant=sys.argv[1]
for schema in ['metal-gpu-intervals','metal-application-intervals','time-profile','ca-client-present-request']:
 p=subprocess.run(['xctrace','export','--input',f'/tmp/aether-{variant}.trace','--xpath',f'/trace-toc/run[@number="1"]/data/table[@schema="{schema}"]','--output',f'/tmp/aether-{variant}-{schema}.xml'],capture_output=True,text=True)
 print(schema,p.returncode,flush=True)
