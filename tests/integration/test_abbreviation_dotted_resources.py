"""Resource validation and CLI guards, also exercised under ASan/UBSan."""
from pathlib import Path
import subprocess
import sys
import tempfile

cli,resources=sys.argv[1:]
with tempfile.TemporaryDirectory(prefix='ttf-dotted-') as directory:
    path=Path(directory)/'customer.tsv'
    def run(rows,text='Read Vol. 3.'):
        path.write_text(rows,encoding='utf-8')
        return subprocess.run([cli,'normalize','--resources',resources,'--language','en-US',
                               '--customer',str(path),text],capture_output=True,text=True,timeout=10)
    for rows in ('abbreviation_dotted\ten-US\tVOL\tvolume\n',
                 'abbreviation_dotted\ten-US\tvol.\tvolume\n',
                 'abbreviation_dotted\ten-US\tvol\t'+'x'*64+'\n',
                 'abbreviation_dotted_period\ten-US\tvol\tunknown\n',
                 'abbreviation_dotted_period\ten-US\tVOL\tsentence\n',
                 'abbreviation_dotted\ten-US\tnewabbr\texpanded\n'):
        result=run(rows)
        assert result.returncode!=0,(rows,result)
    result=run('abbreviation_dotted\ten-US\tnewabbr\texpanded\n'
               'abbreviation\ten-US\tnewabbr\tpreserve\n','newabbr. today')
    assert result.returncode==0 and result.stdout=='expanded today\n',result
    result=run('abbreviation_dotted\ten-US\tnewabbr\tpreserve\n','newabbr.')
    assert result.returncode==0 and result.stdout=='newabbr.\n',result
    result=run('abbreviation_dotted\ten-US\tvol\tpart\n'
               'abbreviation_dotted\ten-US\tvol\tignored\n')
    assert result.returncode==0 and result.stdout=='Read part three.\n',result
    # Same precedence as other resources: customer overrides domain regardless of load order.
    domain=Path(directory)/'domain.tsv'
    domain.write_text('abbreviation_dotted\ten-US\tvol\tbook\n',encoding='utf-8')
    for flags in (['--domain',str(domain),'--customer',str(path)],
                  ['--customer',str(path),'--domain',str(domain)]):
        result=subprocess.run([cli,'normalize','--resources',resources,'--language','en-US',
                               *flags,'Read Vol. 3.'],capture_output=True,text=True,timeout=10)
        assert result.returncode==0 and result.stdout=='Read part three.\n',result
    for text in ('Vol.é', 'ltd.Équipe'):
        result=run('',text)
        assert result.returncode==0 and result.stdout==text+'\n',result
print('dotted resource bounds, boundary dependency, disabling, ties and layer precedence passed')
