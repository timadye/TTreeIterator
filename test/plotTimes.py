#!/usr/bin/env python

from __future__ import print_function
import os, sys, optparse
import ROOT

cols = [ROOT.kBlue, ROOT.kRed, ROOT.kGreen]

def parseArgs():
  parser= optparse.OptionParser(usage="%prog [OPTIONS] timing1a.csv [timing1b.csv...] [ : timing2a.csv [timing2b.csv...] [ : ...]]")
  parser.add_option ("-v", "--verbose",      help="verbose running", action="count", default=0)
  parser.add_option ("-i", "--interactive",  help="ROOT interactive mode", action="store_true")
  parser.add_option ("-o", "--output",       help="plot pdf file")
  parser.add_option ("-l", "--legends",      help="comma-separated list of legends")
  parser.add_option ("-b", "--binning",      help="distribution histogram binning (nbins:tlo:thi)", default="25")
  opt, args= parser.parse_args()
  if not args:
    parser.print_help()
    exit(1)
  if opt.output is None:
    r,e = os.path.splitext(args[0])
    opt.output = r + ".pdf"
  return opt, args

def flat(L):
# if we had a mix of values and lists, then we would need this:
#   return [u for v in [[y for y in x] if type(x) is list else [x] for x in L] for u in v]
  return [u for v in L for u in v]

def process ():
  opt, args = parseArgs()
  if not opt.interactive: ROOT.gROOT.SetBatch(True)
  ROOT.TH1.AddDirectory(0)
  legends = opt.legends.split(",") if opt.legends else []
  nbins, tlo, thi = (opt.binning+":0:0").split(":")[0:3]
  binning = opt.binning.split(":")
  nbins =   int(binning[0]) if len(binning)>=1 else 25
  tlo   = float(binning[1]) if len(binning)>=2 else 0.0
  thi   = float(binning[2]) if len(binning)>=3 else 0.0
  trees = []
  nfiles = []
  add=False
  i = 0
  for f in args:
    if f == ":":
      add=False
      i += 1
      continue
    if not add:
      add=True
      leg = legends[i] if legends and i<len(legends) else f
      tree = ROOT.TTree ("times_%d" % i, leg)
      trees.append(tree)
      nfiles.append(1)
      n = tree.ReadFile(f,"",",")
    else:
      nfiles[-1] += 1
      # skip header line
      fs = ROOT.std.ifstream(f)
      while True:
        c=fs.get()
        if c==10 or c<0: break   # stop on '\n' or end of file
      n = tree.ReadStream(fs,"",",")
      del fs
    if not n: return 1
    print ("read %d entries from file %s into tree '%s' with legend '%s'" % (n, f, tree.GetName(), tree.GetTitle()))

  nt = len(trees)
  sf = 1.0 / float(nt)
  w = min (0.45, 0.65*sf)
  ROOT.gStyle.SetPaintTextFormat(".0f")
  ROOT.gStyle.SetTickLength(0.0)
  ROOT.gStyle.SetErrorX(0)
  ROOT.gStyle.SetOptTitle(0)

  labels = []
  for tree in trees:
    tree.Draw("label","","goff")
    h = tree.GetHistogram()
    if not h:
      print ("element 'label' not found in '%s'" % tree.GetName())
      return 2
    ax = h.GetXaxis()
    for i in range(ax.GetNbins()):
      lab = ax.GetBinLabel(i+1)
      if lab not in flat(labels):
        if i>=len(labels):
          labels.append([lab])
        else:
          labels[i].append(lab)
  labels = flat(labels)

  profiles = []
  allhists = []
  for i,tree in enumerate(trees):
    p = ROOT.TProfile ("profile_%d" % i, "%s;;ns / double" % tree.GetTitle(), len(labels), 0.0, float(len(labels)))
    hists = []
    profiles.append(p)
    ax = p.GetXaxis()
    for b,label in enumerate(labels):
      ax.SetBinLabel (b+1, label)
      h = ROOT.TH1D ("hist_%d_%d" % (i, b), "%s - %s;ns / double;" % (tree.GetTitle(), label), nbins, tlo, thi)
      hists.append(h)
    allhists.append(hists)
    p.SetStats(0)
    p.SetMinimum(0.0)
    p.SetBarWidth(w)
    p.SetMarkerSize (min (1.0, 3.5*w))
    p.SetBarOffset(0.2 + 1.2*w*i)
    p.SetFillColor(cols[i%len(cols)])
    p.GetYaxis().SetTickLength(0.02)
    for e in tree:
      nval = e.entries * e.branches * e.elements
      print ("%s %s %s %-20s %s %-10s %1s %d %d %d %5g %5g (%d)" %
             (tree.GetTitle(), e.time, e.host, e.label, e.testcase, e.test, e.fill, e.entries, e.branches, e.elements, e.ms, e.cpu, nval))
      b = ax.FindBin(e.label)
      if b < 0:
        print("no bin",e.label)
        continue
      ns = 1000000.0 * e.ms / float(nval)
      p.Fill (e.label, ns)
      hists[b-1].Fill (ns)

  if legends:
    leg = ROOT.TLegend (0.7,0.9-0.025*nt,0.9,0.9)
    leg.SetFillStyle(0)
  for i,p in enumerate(profiles):
    ax = p.GetXaxis()
    for b in range(1,p.GetNbinsX()+1):
      print ("%-10s %-20s %5.1f%s ns/double" % (p.GetTitle(), ax.GetBinLabel(b), p.GetBinContent(b), (" +/- %4.1f" % p.GetBinError(b)) if p.GetBinEntries(b)>1 else ""))
    if not i: p.Draw("bar text0")
    else:     p.Draw("bar text0 same")
    if legends and i<len(legends): leg.AddEntry(p,legends[i],"f")
  if legends: leg.Draw()
  printPlot (opt.output, opt.interactive, 1)
  printPlot (opt.output, opt.interactive)
  ROOT.gStyle.SetOptTitle(1)
  for h in flat(allhists):
    h.Draw()
    printPlot (opt.output, opt.interactive)
  printPlot (opt.output, opt.interactive, 2)

def printPlot (output, interactive=False, mult=0):
  if not ROOT.gPad: return
  canvas= ROOT.gPad.GetCanvas()
  if not canvas: return
  if mult:
    if mult==1: canvas.Print(output+"[")
    if mult==2: canvas.Print(output+"]")
    return
  canvas.Print(output)
  if interactive:
    canvas.Update()
    print ("Double-click or press any key in window '"+canvas.GetTitle()+"' to continue...")
    canvas.WaitPrimitive()
  canvas.Clear()

exit (process())
