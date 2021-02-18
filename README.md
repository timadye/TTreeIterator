# TTreeIterator

Creation and Storage of RooFitResults and RooAbsData in TTrees.

The classes currently behave like smart pointers onto an underlying TTree.


### RooFitResultTree

Construct the tree using a `RooAbsPdf` (or `RooStats::ModelConfg`):

```asm
RooFitResultTree frt("myFits", myPdf);
```

And then perform a fit to the pdf using data and global observables:

```asm
std::shared_ptr<RooFitResult> fr = frt.fitTo(data,globs);
```

The `RooFitResultTree` retains fit configuration metadata (in the `UserInfo` of the tree). This includes 
a `TRef` to the pdf, and what RooFit options are used to construct the NLL. The tree ensures 
all fits in the tree are performed with the same fit configuration.

An important feature of the `RooFitResultTree` is that at the time of the first fit the
list of variable (i.e. floatable) parameters is decided. All other parameters are marked as constants and 
will always be held constant as such in subsequent fits with this tree. For this reason it is important 
to declare which parameters you want to be considered as variable and which are fixed.
You can see the list of variable parameters for the tree with:

```asm
frt.Print() // lists model, variables, and fit config
```

If the variables have not yet been defined, you can define them using `SetParameters` with either and explicit
list of parameters or by passing a dataset and global observable list to let it determine the parameters from the model:

````asm
frt.SetParameters(data,globs);
frt.SetParameters(parList); // alternative
````

The constant parameters are taken as `arugments` and the non-constant ones are the `variables`. 
When constructing the tree using a `ModelConfig` the NP and POI are used as the variable parameters.

Prior fits can be obtained with:

```asm
std::shared_ptr<RooFitResult> fr = frt.GetFit(entryNumber);
```


### RooDataTree

Generates toy or expected datasets for a given pdf:

```asm
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

