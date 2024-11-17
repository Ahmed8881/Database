import { Component } from '@angular/core';
import { RouterOutlet } from '@angular/router';
import { FeaturesComponent } from './features/features.component';
import { HeaderComponent } from './header/header.component';
import { FooterComponent } from './footer/footer.component';
import { CtaComponent } from './cta/cta.component';
import { CommonModule } from '@angular/common';
import { HeroComponent } from "./hero/hero.component";
import { PricingComponent } from "./pricing/pricing.component";
@Component({
  selector: 'app-root',
  standalone: true,
  imports: [RouterOutlet, CommonModule, FeaturesComponent, HeaderComponent, FooterComponent, CtaComponent, HeroComponent, PricingComponent],
  templateUrl: './app.component.html',
  styleUrl: './app.component.scss'
})
export class AppComponent {
  title = 'TaskTree';
}
