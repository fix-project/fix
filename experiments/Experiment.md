See [Zenodo]({https://zenodo.org/records/17154970) for the raw data and fix repository dependencies for the original experiments.

# Setup
All experiments were conducted on Amazon EC2 m5.8xlarge instances with Ubuntu 24.10, with 32 vCPU cores and 128 GiB memory.

After setting up Fixpoint according to the top-level README.md, there are serveral components for setting up the environment for each experiment:

* The content of the working directory of the client
* The content of the working directory of the server(s)
* A file that contains the list of servers in the format of

```
<address1>:<port1>
<address2>:<port2>
<address3>:<port3>
...
```
* The script for running the servers and the client

All scripts assume that the Fixpoint source directory is at `~/fix`.

Since the working directory data is too large for GitHub, the full setup with all data is temporarily hosted on Google Drive. Please see artifact appendix for the link.

# List of experiments
* Invocation overhead (Sec 5.2.1): `lwvirt`
* Orchetration overhead (Sec 5.2.2): `function-chain`
* I/O externalization with one-off functions (Sec 5.3.1): `externalio`
* I/O externalization with locality (Sec 5.3.2): `count-words`
* Fine-grained function invocations (Sec 5.4): `bptree-get`
* Burst-parallel applications (Sec 5.5): `compile`

# Instruction 
For each experiment, `data/` contains the raw data collected for the paper, and `README.md` contains the instruction for running the experiment. For all experiments, the fix servers need to ended and restarted between runs.
