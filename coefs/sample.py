import os
import yaml
import pyEXP

os.chdir('/home/weinberg/Nbody/Better')

yaml_config = ""
with open('basis.yaml') as f:
    config = yaml.load(f, Loader=yaml.FullLoader)
    yaml_config = yaml.dump(config)

basis   = pyEXP.basis.Basis.factory(yaml_config)
batches = pyEXP.pread.ParticleReader.parseFileList('file.list', '')
coefs   = None

for group in batches:

    print("file group is", group)

    # Make the reader for the desired type.  One could probably try to
    # do this by inspection but that's another project.
    #
    reader = pyEXP.pread.ParticleReader.createReader('PSPout', group, 0, False);

    # Print the type list
    #
    print('The component names are:', reader.GetTypes())

    compname = 'dark halo'
    reader.SelectType(compname)
    print('Selected', compname)

    print('Call createCoefficients at Time', reader.CurrentTime(), 'for', reader.CurrentNumber(), 'particles')

    coef = basis.createCoefficients(reader)
    print("Created coef")

    # We need this stupid idiom here because None is not mapping to a
    # null pointer.  There is probably a way to fix this.
    if coefs is None:
        coefs = pyEXP.coefs.Coefs.makecoefs(coef)
    else:
        coefs.add(coef)

    print('Added coef')
    print('-'*60)

print('\nCompleted the file group list\n')

print('The coefficient time list is', coefs.Times())
