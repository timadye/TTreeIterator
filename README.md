# TTreeIterator

Creation and Storage of RooFitResults and RooAbsData in TTrees.

The classes currently behave like smart pointers onto an underlying TTree.


### RooFitResultTree

Construct the tree using a `RooAbsPdf` (or `RooStats::ModelConfg`):

```
RooFitResultTree frt("myFits", myPdf);
```

And then perform a fit to the pdf using data and global observables:

```
std::shared_ptr<RooFitResult> fr = frt.fitTo(data,globs);
```

The `RooFitResultTree` retains fit configuration metadata (in the `UserInfo` of the tree). This includes 
a `TRef` to the pdf, and what RooFit options are used to construct the NLL. The tree ensures 
all fits in the tree are performed with the same fit configuration.

Prior fits can be obtained with:

```
std::shared_ptr<RooFitResult> fr = frt.GetFit(entryNumber);
```


### RooDataTree

Generates toy or expected datasets for a given pdf:

```
RooDataTree dt("myToys", myPdf);
```

A `RooFitResult` is required in order to generate a dataset. The fit result defines the 
values of the model parameters at which the generation occurs. To support generating toys at an arbitrary 
parameter set, the `RooFitResultTree` can be used to generate a `RooFitResult` where all parameters are constant 
(no fit is therefore run): `frt.snapshotTo()`

```asm
pair<shared_ptr<RooAbsData>,shared_ptr<const RooArgSet>> toy = 
       dt.generate(fr, expected /* if true generates expected data */)
```

the first in the pair is the dataset, the second is the global observables. In order to generate 
global observables, the tree must have been configured with:

```asm
dt.SetGlobalObservables(globs);
```

