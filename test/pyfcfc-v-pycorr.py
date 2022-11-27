import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import sys, os
sys.path.append("/global/homes/d/dforero/codes/pyfcfc/")
from pyfcfc.sky import py_compute_cf
from pyfcfc.utils import add_pair_counts, compute_multipoles
import pandas as pd
import time
from pycorr import TwoPointCorrelationFunction, setup_logging
from astropy.table import Table, vstack
from pyrecon import IterativeFFTReconstruction, MultiGridReconstruction, utils
from cosmoprimo.fiducial import DESI


setup_logging()

def get_maskbit(main=0, nz=0, Y5=0, sv3=0):
    return main * (2**3) + sv3 * (2**2) + Y5 * (2**1) + nz * (2**0)

def mask_bit_z(tab, main, nz, Y5, sv3):
    maskbit = get_maskbit(main, nz, Y5, sv3)
    tab = tab[(tab['STATUS'] & maskbit == maskbit)]
    return tab

def load_single(args):
    f, main, nz, Y5, sv3, cap, zmin, zmax = args
    print(zmin, zmax, cap, flush=True)
    return mask_redshift(mask_cap(mask_bit_z(Table.read(f), main, nz, Y5, sv3), cap), zmin, zmax)
def fits_concat_reader_wselect(files, main, nz, Y5, sv3, cap, columns = None, zmin=0.8, zmax=1):
    args_list = []
    for f in files:
        args_list.append((f, main, nz, Y5, sv3, cap, zmin, zmax))
    
    import multiprocessing as mp
    pool = mp.Pool(len(files))
    tables = pool.map(load_single, args_list)
    pool.close()

    print("Stacking randoms", flush=True)
    if columns is None:
        return vstack(tables, metadata_conflicts = 'silent').to_pandas().dropna()
    else:
        return vstack(tables, metadata_conflicts = 'silent').to_pandas().dropna()[columns]

def mask_cap(tab, cap='SGC'):
    cap_mask = (tab['RA'] > 84.) & (tab['RA'] < 303.25)
    #print(cap_mask.sum())
    if cap == 'SGC':
        tab = tab[~cap_mask]
    else:
        tab = tab[cap_mask]
    
    return tab
def mask_redshift(tab, zmin, zmax):
    z_mask = (tab['Z'] < zmax) & (tab['Z'] > zmin)
    #print(z_mask.sum())
    return tab[z_mask]

TRACER = "LRG"
REDSHIFT = 0.8
DATA_ROOT = f"/global/cfs/cdirs/desi/cosmosim/FirstGenMocks/EZmock/CutSky_6Gpc/{TRACER}/z{REDSHIFT:.3f}/"
RAND_ROOT = f"/global/cfs/cdirs/desi/cosmosim/FirstGenMocks/EZmock/CutSky_6Gpc/{TRACER}/Random/"
#print(DATA_ROOT)
cosmo = DESI()
distance_to_redshift = utils.DistanceToRedshift(distance = cosmo.comoving_radial_distance)
seed = 1002
cap ='NGC'
zmin, zmax = 0.8, 1.1
f = 0.830
P0 = 1e4
n_rand_splits = 20
edges = np.arange(0, 201, 1, dtype=np.double), np.linspace(-1, 1, 101)
nthreads = 256
fig, ax = plt.subplots(nrows=1, ncols=3, figsize=(10,5))

print("Loading randoms...", flush = True)
randoms_list = [f"{RAND_ROOT}/cutsky_LRG_S{i}_{cap}.fits" for i in range(1000, 3000, 100)]
rand = fits_concat_reader_wselect(randoms_list, main=1, nz=0, Y5=1, sv3=0, cap = cap, columns = ['RA', 'DEC', 'Z', 'NZ_MAIN'], zmin = zmin, zmax = zmax)
wrand = (1 / (1 + P0 * rand['NZ_MAIN']).values).astype(np.float32)


file_dir = f"{DATA_ROOT}/cutsky_{TRACER}_z{REDSHIFT:.3f}_EZmock_B6000G1536Z0.8N216424548_b0.385d4r169c0.3_seed{seed}"
#print(MPI.COMM_WORLD.Get_rank(), file_dir)
output_base = f"/global/homes/d/dforero/projects/ezmock-y1kp4-recon/data/pyrecon/{file_dir.replace('/global/cfs/cdirs/desi/cosmosim/FirstGenMocks/', '')}/{cap}/{zmin:.1f}z{zmax:.1f}f{f:.3f}/"
os.makedirs(output_base, exist_ok = True)
output_dat = f"{output_base}/recon_dat.npy"
output_sym = f"{output_base}/recon_sym.npy"
output_iso = f"{output_base}/recon_iso.npy"
output_cf_iso = f"{output_base}/iso_tpcf.pkl.npy"
output_cf_sym = f"{output_base}/sym_tpcf.pkl.npy"




data = np.load(output_dat)
wdata = (1. / (data[:,3] * P0 + 1)).astype(np.float32)
shifted = np.load(output_sym)
wshifted = (1. / (shifted[:,3] * P0 + 1)).astype(np.float32)

h = 0.6736
s = time.time()
total_results = {}
for i, (_shifted, _rand, _wshifted, _wrand) in enumerate(zip(*map(lambda x: np.array_split(x, n_rand_splits), (shifted, rand[['RA', 'DEC', 'Z']].values, wshifted, wrand)))):
    s_ = time.time()
    results = py_compute_cf([data, _rand, _shifted], 
                            [wdata, _wrand, _wshifted], 
                            edges[0], None, edges[1].shape[0], 
                            label = ['D', 'R', 'S'] if i == 0 else ['D', 'S'], 
                            omega_m = (0.02237 + 0.1200) / h**2, 
                            #omega_l = 0.69, 
                            eos_w = -1, 
                            bin = 1, 
                            pair = ['DD', 'RR', 'DS', 'SS'] if i == 0 else ['DS', 'SS'], 
                            cf = ['DS'], 
                            multipole = [0,2,4], 
                            convert = 'T',
                            data_struct = 0,
                            verbose = 'F')
    
    #total_results = add_pair_counts(total_results, results) if i > 0 else results
    total_results = results
    break
    
    print(f"pyfcfc single split {time.time() - s_}s", flush=True)
print(f"pyfcfc all splits {time.time() - s}s", flush=True)
total_results['cf'] = (total_results['pairs']['DD'] - 2 * total_results['pairs']['DS'] + total_results['pairs']['SS']) / total_results['pairs']['RR']
total_results['multipoles'] = compute_multipoles(total_results['cf'], [0,2,4])
for i in range(3):
    ax[i].plot(total_results['s'], total_results['s']**2*total_results['multipoles'][i,:])
    ax[i].set_xlabel("$s$ [Mpc/$h$]")
    ax[i].set_ylabel(r"$s^2\xi$")
    ax[i].set_title(f"$\ell = {2*i}$")
fig.savefig("test/pyfcfc-v-pycorr.png", dpi=300)

rand = rand['RA'].values, rand['DEC'].values, cosmo.comoving_radial_distance(rand['Z'].values)
split_rand = tuple(map(lambda x: np.array_split(x, n_rand_splits), rand))
split_wrand = np.array_split(wrand, n_rand_splits)


data = [data[:,0], data[:,1], cosmo.comoving_radial_distance(data[:,2])]
shifted = shifted[:,0], shifted[:,1], cosmo.comoving_radial_distance(shifted[:,2])



s = time.time()
split_shifted = tuple(map(lambda x: np.array_split(x, n_rand_splits), shifted))
split_wshifted = np.array_split(wshifted, n_rand_splits)
D1D2 = None
R1R2 = None
s = time.time()
for i in range(n_rand_splits):
    _shifted = list(split_shifted[j][i] for j in range(3))
    _wshifted = split_wshifted[i]
    _rand = list(split_rand[j][i] for j in range(3))
    _wrand = split_wrand[i]    
    print(f"Shifted shape = {_shifted[0].shape[0] / data[0].shape[0]}x")
    print(f"Rand shape = {_rand[0].shape[0] / data[0].shape[0]}x")
    s_ = time.time()
    result = TwoPointCorrelationFunction('smu', edges, data_positions1=data, data_weights1=wdata,
                            data_positions2=None, data_weights2=None,
                            randoms_positions1=_rand, randoms_weights1=_wrand,
                            randoms_positions2=None, randoms_weights2=None,
                            shifted_positions1 = _shifted,
                            shifted_weights1 = _wshifted,
                            engine='corrfunc', nthreads=nthreads,
                            position_type = 'rdd',
                            los = 'firstpoint',
                            D1D2 = D1D2,
                            R1R2 = R1R2
                            #estimator = 'landyszalay'
                            )
    print(f"pycorr single split {time.time() - s_}s", flush=True)
    D1D2 = result.D1D2
    R1R2 = result.R1R2
    break
print(f"pycorr all splits {time.time() - s}s", flush=True)

 

for ill, ell in enumerate([2 * i for i in range(3)]):
    s, corr = result(ell=ell, return_sep=True)
    ax[ill].plot(s, s**2 * corr, label=r'$\ell = {:d}$'.format(ell))

fig.savefig("test/pyfcfc-v-pycorr.png", dpi=300)