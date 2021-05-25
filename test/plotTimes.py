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
  opt, args= parser.parse_args()
  if not args:
    parser.print_help()
    exit(1)
  if opt.output is None:
    r,e = os.path.splitext(args[0])
    opt.output = r + ".pdf"
  return opt, args

def process ():
  opt, args = parseArgs()
  if not opt.interactive: ROOT.gROOT.SetBatch(True)
  ROOT.TH1.AddDirectory(0)
  legends = opt.legends.split(",") if opt.legends else []
  trees = []
  add=False
  i = 0
  for f in args:
    if f == ":":
      add=False
      i += 1
      continue
    if not add:
      add=True
      tree = ROOT.TTree ("times_%d" % i, f)
      trees.append(tree)
      n = tree.ReadFile(f,"",",")
    else:
      # skip header line
      fs = ROOT.std.ifstream(f)
      while True:
        c=fs.get()
        if c==10 or c<0: break   # stop on '\n' or end of file
      n = tree.ReadStream(fs,"",",")
      del fs
    if not n: return 1
    print ("read %d entries from file %s into tree '%s'" % (n, f, tree.GetName()))
  nt = len(trees)
  sf = 1.0 / float(nt)
  w = min (0.45, 0.55*sf)
  ROOT.gStyle.SetPaintTextFormat(".0f")
  ROOT.gStyle.SetTickLength(0.0)
  tree0 = trees[0]
  tree0.Draw("label","","goff")
  h0 = tree0.GetHistogram()
  if not h0:
    print ("element 'label' not found in '%s'" % tree0.GetName())
    return 2
  h0.Reset("M")
  ax = h0.GetXaxis()
  hists = []
  for i,tree in enumerate(trees):
    h = ROOT.TProfile ("hist_%d" % i, tree.GetTitle(), h0.GetNbinsX(), 0.0, float(h0.GetNbinsX()))
    hists.append(h)
    for bin in range(1,h0.GetNbinsX()+1):
      h.GetXaxis().SetBinLabel (bin, ax.GetBinLabel(bin))
    h.SetStats(0)
    h.SetTitle(";;ns / double")
    h.SetMinimum(0.0)
    h.SetBarWidth(w)
    h.SetMarkerSize (min (1.0, 3.5*w))
    h.SetBarOffset(0.3 + 1.3*w*i)
    h.SetFillColor(cols[i%len(cols)])
    for e in tree:
      nval = e.entries * e.branches * e.elements
      bin = ax.FindBin(e.label)
      print ("%s:%d %s %s %-20s %s %-10s %1s %d %d %d %5g %5g (%d)" %
             (tree.GetName(), bin, e.time, e.host, e.label, e.testcase, e.test, e.fill, e.entries, e.branches, e.elements, e.ms, e.cpu, nval))
      if bin < 0:
        print("no bin",e.label)
        continue
      h.Fill (e.label, 1000000.0 * e.ms / float(nval))
  if legends:
    leg = ROOT.TLegend (0.8,0.85,0.9,0.9)
    leg.SetFillStyle(0)
  for i,h in enumerate(hists):
    if not i: h.Draw("bar text0")
    else:     h.Draw("bar text0 same")
    if legends and i<len(legends): leg.AddEntry(h,legends[i],"f")
  if legends: leg.Draw()
  if opt.interactive: waitPlot()
  else: ROOT.gPad.Print(opt.output)


def waitPlot():
  if not ROOT.gPad: return
  canvas= ROOT.gPad.GetCanvas()
  if not canvas: return
  print ("Double-click or press any key in window '"+canvas.GetTitle()+"' to continue...")
  canvas.WaitPrimitive()

exit (process())
